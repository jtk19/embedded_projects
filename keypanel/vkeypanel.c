#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/timer.h>

#include "insync-ssc.h"
#include "vkeypanel.h"


/* uncomment line below for production; comment out for testing & debugging */
#define NDEBUG



// for serialized access to the SSC
static DEFINE_SPINLOCK( user_lock );
static LIST_HEAD( ssc_list );


// main device structure
struct insync_keypanel
{
	struct list_head	list;
	int					user;

	struct platform_device 	*pdev;
	int					 irq;

	struct input_dev	*idev;
	char				 phys[32];

	unsigned int 	tx_frame;		
	unsigned int	rx_frame;		// the current RX frame received
	unsigned int 	prev_rx_frame;	// the previous RX frame received
};


/*------------------------------------------------------------------------
 * management of serial access to the SSC
 */

struct insync_keypanel *in_ssc_request(unsigned int ssc_num)
{
	int ssc_valid = 0;
	struct insync_keypanel *ssc;

	spin_lock(&user_lock);
	list_for_each_entry(ssc, &ssc_list, list) 
	{
		if (ssc->pdev->id == ssc_num) 
		{
			ssc_valid = 1;
			break;
		}
	}

	if (!ssc_valid) 
	{
		spin_unlock(&user_lock);
		pr_err("ssc: ssc%d platform device is missing\n", ssc_num);
		return ERR_PTR(-ENODEV);
	}

	if (ssc->user) 
	{
		spin_unlock(&user_lock);
		dev_dbg(&ssc->pdev->dev, "module busy\n");
		return ERR_PTR(-EBUSY);
	}
	ssc->user++;
	spin_unlock(&user_lock);

	return ssc;
}


void in_ssc_free(struct insync_keypanel *ssc)
{
	spin_lock(&user_lock);
	if (ssc->user) {
		ssc->user--;
	} else {
		dev_dbg(&ssc->pdev->dev, "device already free\n");
	}
	spin_unlock(&user_lock);
}


/*------------------------------------------------------------------------
 * The ISR & supporting functions
 */

unsigned int RX_HOLDING_REGISTER;
spinlock_t	 RHR_lock = SPIN_LOCK_UNLOCKED;


static inline void process_RX_data_frame( struct insync_keypanel *keypanel )
{
	unsigned int key_states, prev_key_states;
//	unsigned int key_state, prev_key_state;
	unsigned int encoder;
	unsigned int xor, mask;
	unsigned short key_id = 0, max;
	unsigned int evalue = 0;	// event ABS_Y value

	keypanel->prev_rx_frame = keypanel->rx_frame;

	spin_lock( &RHR_lock );
	keypanel->rx_frame = RX_HOLDING_REGISTER;	// read the data frame from the Receive Hold Register
	RX_HOLDING_REGISTER = 0x00000000;
	spin_unlock( &RHR_lock );

	// process any key events
	
	key_states = keypanel->rx_frame & IN_KP_KEY_STATES_MASK;
	prev_key_states = keypanel->prev_rx_frame & IN_KP_KEY_STATES_MASK;

	/*
	if ( key_states ^ prev_key_states )	// XOR: proceed if only there is some change 
	{
		for ( key_id = 0; key_id < IN_KP_NUM_KEYS; ++key_id )
		{
			key_state = key_states & IN_KP_KEY_MASK( key_id );
			prev_key_state = prev_key_states & IN_KP_KEY_MASK( key_id );

			if ( key_state ^ prev_key_state ) // XOR; this key state has changed
			{
				if ( key_state ) // non-zero, this is a KEY_DOWN event
				{
					evalue = ( IN_KP_EVENT_KEY_DOWN << IN_KP_EVENT_EVENT_ID_OFFSET )
							| key_id;
				}
				else 			// this is a KEY_UP event
				{
					evalue = ( IN_KP_EVENT_KEY_UP << IN_KP_EVENT_EVENT_ID_OFFSET )
							| key_id;
				}

				// send up ths key event into user space
				input_report_abs( keypanel->idev, ABS_Y, evalue );
				input_sync( keypanel->idev );
			}
		}
	}
	*/


	xor = key_states ^ prev_key_states;

	max = ( xor < 1024 ) ? 10 : IN_KP_NUM_KEYS;	

	while ( xor   &&   key_id < max ) // xor non zero => there's some bit change to process still
	{
		mask = 1 << key_id;
		if ( xor & mask )	// this bit xor is non zero; bit key_id has changed
		{
			if ( key_states & mask ) // non zero: bit key_id is 1, this is a KEY_DOWN event
			{
				evalue = ( IN_KP_EVENT_KEY_DOWN << IN_KP_EVENT_EVENT_ID_OFFSET ) | key_id;
			}
			else
			{	
				evalue = ( IN_KP_EVENT_KEY_UP << IN_KP_EVENT_EVENT_ID_OFFSET ) | key_id;
			}

			// dispatch this key event into user space
			input_report_abs( keypanel->idev, ABS_Y, evalue );
			input_sync( keypanel->idev );
			printk( "[event] dispatched ABS_Y %d (0x%X) for key/event ( %d, %s )\n",
					evalue, evalue, key_id, (evalue > 31) ? "KEY_DOWN" : "KEY_UP" );

			xor &= ~mask;	// flip the processed bit to 0 in xor
		}
		++key_id;		
	}


	// process any rotary encoder events
	encoder = ( keypanel->rx_frame & IN_KP_ROTARY_ENCODER_MASK );
	if ( encoder )	// non-zero, i.e. there is a rotation
	{
		// store the signed rotary magnitude in the second byte
		evalue = encoder >> ( IN_KP_ROTARY_ENCODER_OFFSET - IN_KP_EVENT_ROTARY_VALUE_OFFSET );
		
		// store the rotary encoder id in the least significant 5 bits of the first byte
		evalue |= IN_KP_ROTARY_ENCODER;
		
		// store the event id in the last (most significant) 3 bits of the first byte
		if ( encoder > 0 )  // right turn
		{
			evalue |= ( IN_KP_EVENT_ROTARY_RIGHT_TURN << IN_KP_EVENT_EVENT_ID_OFFSET );
		}
		else
		{
			evalue |= ( IN_KP_EVENT_ROTARY_LEFT_TURN << IN_KP_EVENT_EVENT_ID_OFFSET );
		}

		// send up the rotary event into user space
		input_report_abs( keypanel->idev, ABS_Y, evalue );
		input_sync( keypanel->idev );

	} // end if (rotary-encoder)

}

// The ISR 
//static irqreturn_t insync_keypanel_interrupt(int irq, void *dev)
static void insync_keypanel_interrupt( unsigned long data )
{
	struct insync_keypanel 	*keypanel;
	unsigned short tries = 0;

	spin_lock( &RHR_lock );
	RX_HOLDING_REGISTER |= (unsigned int)data;
	printk( "RX frame: 0x%X\n", RX_HOLDING_REGISTER );
	spin_unlock( &RHR_lock );
	
	keypanel = in_ssc_request( 0 );
	while ( keypanel == ERR_PTR(-ENODEV)  ||  keypanel == ERR_PTR(-EBUSY) )
	{
		keypanel = in_ssc_request( 0 );
		++tries;
		if ( tries > 7 )
		{
			printk( "insync_keypanel_interrupt() failed: could not get a handle on ssc0. " );
			printk( "RX frame [0x%x] lost.\n", (int)data );
			return;
		}
		printk( "try [%d]  ", tries);
		if ( keypanel == ERR_PTR(-ENODEV)  ||  keypanel == ERR_PTR(-EBUSY) )
			mdelay( 1 ); // 1 millisecond wait
	}

	process_RX_data_frame( keypanel );
	
	in_ssc_free( keypanel );

}

struct timer_list tm = {
	.expires = 100,
	.function = insync_keypanel_interrupt,
};

/*------------------------------------------------------------------------
 * The sysfs group for the input of the LED driving TX frame
 */

static ssize_t write_LED_dataframe( struct device *dev,
									struct device_attribute *attr,
									const char *buffer, size_t count )
{
	struct insync_keypanel *keypanel = ERR_PTR(-ENODEV) ;
	unsigned int LED_states;
	int rtn, tries =  0;
	

	rtn = sscanf( buffer, "%x", &LED_states );
	if ( rtn != 1 ) // some error
	{
		printk( "LED data error in [%s]\n", buffer );
		return -1;
	}
	printk( "LED state data received: 0x%X\n", LED_states );
	printk( "Timer HZ : %d\n", HZ );

	// make sure only data sets only the valid or active LED states
	LED_states &= IN_KP_LED_ACTIVE_STATES_MASK;

	keypanel = in_ssc_request( 0 );
	while ( keypanel == ERR_PTR(-ENODEV)  ||  keypanel == ERR_PTR(-EBUSY) )
	{
		keypanel = in_ssc_request( 0 );
		++tries;
		if ( tries > 7 )
		{
			printk( "write_LED_dataframe() failed: could not get a handle on ssc0. " );
			printk( "TX frame [0x%x] lost.\n", LED_states );
			return -1;
		}
		printk( "try [%d], ", tries);
		mdelay( 1 ); // 1 millisecond wait
	}

//	ssc_writel( keypanel->regs, THR, LED_states );
	printk( "\nLED states written : 0x%X\n", LED_states );
	printk( "Device phys is %s.\n", keypanel->phys );

	in_ssc_free( keypanel );

	return count;
}

// write permissions only into the device, for TX data frames; 
// reading from the device is not allowed;
// RX data is read from the device's event interface
DEVICE_ATTR( TX_DATA, 0220, NULL, write_LED_dataframe );

static struct attribute *keypanel_attrs[] = {
	&dev_attr_TX_DATA.attr,
	NULL
};

static struct attribute_group keypanel_attrg =
{
	.attrs = keypanel_attrs,
};


/*------------------------------------------------------------------------
 * The sysfs group for the input of the RX button event simulation
 */

struct timer_list tm_up1;
struct timer_list tm_down[3];
struct timer_list tm_up[3];

static inline void first_press( unsigned int key )
{
	init_timer( &tm_up1 );
	tm_up1.function = insync_keypanel_interrupt;
	tm_up1.data = 0;
	tm_up1.expires = jiffies + IN_SIM_KEY_UP_TIME; 			
	
	insync_keypanel_interrupt( 1 << key ); 		// KEY_DOWN
	add_timer( &tm_up1 );						// KEY_UP
}

static inline void press_and_hold( unsigned int key )
{
	init_timer( &tm_up1 );
	tm_up1.function = insync_keypanel_interrupt;
	tm_up1.data = 0;
	tm_up1.expires = jiffies + IN_SIM_HOLD_UP_TIME; 			
	
	insync_keypanel_interrupt( 1 << key ); 		// KEY_DOWN
	add_timer( &tm_up1 );						// KEY_UP
}

static inline void press( unsigned short press_id, unsigned int key )
{
	unsigned short i = press_id - 2;
	printk( "press %d\n", press_id );

	init_timer( &tm_down[ i ] );
	init_timer( &tm_up[ i ] );

	tm_down[i].function = tm_up[i].function = insync_keypanel_interrupt;
	tm_down[i].data = ( 1 << key );
	tm_up[i].data = 0;
	tm_down[i].expires = jiffies + (press_id -1 ) * IN_SIM_NEXT_PRESS_TIME; 
	tm_up[i].expires = jiffies + 
				(press_id -1)*IN_SIM_NEXT_PRESS_TIME + IN_SIM_KEY_UP_TIME; 			

	printk( "Press %d KEY_DOWN timer: %d\n", press_id, 
			(press_id -1 ) * IN_SIM_NEXT_PRESS_TIME );
	printk( "Press %d KEY_UP timer: %d\n", press_id, 
			(press_id -1 )*IN_SIM_NEXT_PRESS_TIME + IN_SIM_KEY_UP_TIME );
	
	add_timer( &tm_up[i] );		// KEY_UP
	add_timer( &tm_down[i] );	// KEY_DOWN
}


static void simulate_key_event( unsigned int key, unsigned int kevent )
{

	printk( "Timer HZ : %d.   ", HZ );
	printk( "processing RX frame : 0x%X\n", 1 << key );

	switch ( kevent )
	{
		case IN_SINGLE_PRESS: 	first_press( key );
								break;

		case IN_DOUBLE_PRESS:	first_press( key );
								press( 2, key );
								break;

		case IN_TRIPLE_PRESS:	first_press( key );
								press( 2, key );
								press( 3, key );
								break;

		case IN_QUAD_PRESS	:	first_press( key );
								press( 2, key );
								press( 3, key );
								press( 4, key );
								break;

		case IN_PRESS_AND_HOLD:	press_and_hold( key );
								break;
	}
}

static ssize_t write_RX_data(   struct device *dev,
								struct device_attribute *attr,
								const char *buffer, size_t count )
{
	struct insync_keypanel *keypanel = ERR_PTR(-ENODEV) ;
	int key, kevent;
	int rtn, tries =  0;
	

	rtn = sscanf( buffer, "%d,%d", &key, &kevent );
	if ( rtn != 2 ) // some error
	{
		printk( "RX data error in [%s]\n", buffer );
		return -1;
	}
	printk( "RX simulation data received: [ %d, %d ]\n", key, kevent );

	keypanel = in_ssc_request( 0 );
	while ( keypanel == ERR_PTR(-ENODEV)  ||  keypanel == ERR_PTR(-EBUSY) )
	{
		keypanel = in_ssc_request( 0 );
		++tries;
		if ( tries > 7 )
		{
			printk( "write_LED_dataframe() failed: could not get a handle on ssc0. " );
			printk( "Simulated RX key event [ %d, %d ] lost.\n", key, kevent );
			in_ssc_free( keypanel );
			return -1;
		}
		printk( "try [%d], ", tries);
	}
	printk( "Device phys is %s.\n", keypanel->phys );

	in_ssc_free( keypanel );

	simulate_key_event( key, kevent );

	return count;
}

// write permissions only into the device, for TX data frames; 
// reading from the device is not allowed;
// RX data is read from the device's event interface
DEVICE_ATTR( RX_DATA, 0220, NULL, write_RX_data );

static struct attribute *RX_attrs[] = {
	&dev_attr_RX_DATA.attr,
	NULL
};

static struct attribute_group RX_attrg =
{
	.attrs = RX_attrs,
};



/*------------------------------------------------------------------------
 * Virtual SSC KeyPanel platform driver
 */

static int in_keypanel_probe( struct platform_device *pdev )
{
//	struct resource 			*res;
	struct insync_keypanel		*keypanel;
	struct input_dev			*input;
//	unsigned int	mck, clk_div;
//	unsigned int	reg;
	int	rtn = 0;
	
				
	// allocate memory for our device
	keypanel = kzalloc( sizeof( struct insync_keypanel ), GFP_KERNEL );
	if ( !keypanel )
	{
		dev_err( &pdev->dev, "probe(): memory allocation for insync_keypanel failed\n");
		return -ENOMEM;
	}
	keypanel->pdev = pdev;
	printk( "probe(): insync_keypanel device allocated\n" );


	// allocate input group device
	input = input_allocate_device();
	if ( !input )
	{
		dev_err( &pdev->dev, "probe(): failed to allocate input device\n");
		rtn = -EBUSY;
		goto l_exit_free_mem;
	}
	keypanel->idev = input;
	printk( "probe(): input device allocated\n" );

/*	
	// set up the interrupts 
	keypanel->irq = platform_get_irq(pdev, 0);
	if ( keypanel->irq < 0 ) 
	{
		dev_err( &pdev->dev, "probe(): no irq ID is designated.\n");
		rtn = -ENODEV;
		goto l_exit_free_idev;
	}

	// register the ISR
	rtn = request_irq( keypanel->irq, insync_keypanel_interrupt, IRQF_DISABLED,
			pdev->dev.driver->name, keypanel);
	if ( rtn ) 
	{
		dev_err( &pdev->dev, "insync_adcc: failed to allocate irq.\n");
		goto l_exit_free_idev;
	}
 */


	// initialise the keypanel device and configure the input device
	keypanel->rx_frame = 0x00000000;
	keypanel->prev_rx_frame = 0x00000000;
	snprintf( keypanel->phys, sizeof( keypanel->phys ), 
			"%s/input0", dev_name(&pdev->dev) );
	printk( "probe(): keypanel data initialized\n" );
	
	input->name = "Virtual SSC KeyPanel Controller";
	input->uniq = "vkeypanel";
	input->phys = keypanel->phys;
	input->dev.parent = &pdev->dev;

	__set_bit( EV_ABS, input->evbit );
	input_set_abs_params( input, ABS_Y, 0, 0xFFFF, 0, 0); // value sent up is 16 bits

	printk( "probe(): input device initialized\n" );

	/*
	// the clock settings
	clk_enable( keypanel->clk );
	
	mck = clk_get_rate( keypanel->clk );
	dev_info( &pdev->dev, "Master clock is set at: %d Hz\n", mck );
	
	clk_div = (int) ( mck / ( 2 * IN_SSC_DESIRED_CLK_SPEED_HZ ) );
	clk_div = ( clk_div > 0 ) ? clk_div : 1;
	clk_div = ( clk_div > IN_SSC_MAX_CLK_DIV ) ? IN_SSC_MAX_CLK_DIV : clk_div;

	dev_info( &pdev->dev, "SSC CLK DIV is set to: %d\n", clk_div );
	dev_info( &pdev->dev, "SSC CLK is being set to: %d Hz\n", 
							(int) ( mck / ( 2 * clk_div ) ) );


	// the register setup
	reg = 	( 0x1 << SSC_CR_RXEN_OFFSET ) |			// receive enable
		  	( 0x1 << SSC_CR_TXEN_OFFSET ) |			// transmit enable	
			( 0x1 << SSC_CR_SWRST_OFFSET );			// software reset
	ssc_writel( keypanel->regs, CR, reg );			// write Control Register SSC_CR

	ssc_writel( keypanel->regs, CMR, clk_div );		// clocks driven off the MCK drivider

	//  configure RX  
	// RX Clock Mode Register: start receiving data on a new write on Compare 0
	// others RCMR defaults: driven off MCK div, no RK, continuos clocked etc
//	reg = 0x00000000 | ( IN_RX_START_ON_COMPARE0 << SSC_RCMR_START_OFFSET) ;
	ssc_writel( keypanel->regs, RCMR, 0x00000000 );	// all defaults, continuous data receipt	

	// RX Frame Mode Register: data length = 32 (31+1) bits
	reg = 0x00000000 |
			( IN_KP_RX_FRAME_LEN_LESS1 << SSC_RFMR_DATLEN_OFFSET ) |
			( 0x0 	<< SSC_RFMR_MSBF_OFFSET ) |				// little endian - LSB first
			( IN_KP_NUM_FRAME_SYNC_BITS << SSC_RFMR_FSLEN_OFFSET );	// synchronized by 1 single bit "0"
	ssc_writel( keypanel->regs, RFMR, reg );


	//  configure TX  
	// TX Clock Mode Register: all defaults - driven off MCK divider, continous clocked
	// no output TK, start transmision as soon as SSC_THR is written with the data
	ssc_writel( keypanel->regs, TCMR, 0x00000000 );

	// TX frame Mode Register
	reg = 0x00000000 |
			( IN_KP_TX_FRAME_LEN_LESS1 << SSC_TFMR_DATLEN_OFFSET ) | // 32 (31+1) bit data frame
			( 0x1 	<< SSC_TFMR_DATDEF_OFFSET ) |			// drive TD with "1" while quiescent
			( 0x0	<< SSC_TFMR_MSBF_OFFSET ) |				// little endian - LSB first
			( IN_KP_NUM_FRAME_SYNC_BITS << SSC_TFMR_FSLEN_OFFSET ) |	// one single synchronization bit "0"
			( 0x1 	<< SSC_TFMR_FSDEN_OFFSET );				// shift out this "0" sync bit from TSHR
	ssc_writel( keypanel->regs, TFMR, reg );
	ssc_writel( keypanel->regs, TSHR, 0x00000000 );			// sync bit is "0"


	//  configure the interrupts 
	reg = 0x00000000 |
			( 0x1 	<< SSC_IER_ENDRX_OFFSET ) |		// enable end-of-reception interrupt for the frame
			( 0x1 	<< SSC_IER_OVRUN_OFFSET ) ;		// alert us if RX data is overwritten before we can read

#ifndef NDEBUG
	reg |=	( 0x1	<< SSC_IER_ENDTX_OFFSET ) ;		// success alert at end of transmission
											// only for debugging purposes, diable for production
#endif
	
	ssc_writel( keypanel->regs, IER, reg );		// enable the above interrutps

	ssc_readl( keypanel->regs, SR );

*/
	platform_set_drvdata( pdev, keypanel);

	rtn = input_register_device( input );
	if ( rtn )
	{
		printk( "probe(): input register device failed\n" );
		rtn = -EBUSY;
		goto l_exit_free_idev;
	}
	printk( "probe(): input device registered\n" );

	spin_lock( &user_lock );
	list_add_tail( &keypanel->list, &ssc_list);
	spin_unlock( &user_lock );
	printk( "probe: list added\n" );

	//dev_info( &pdev->dev, "Virtual SSC KeyPanel initialized (irq %d).\n", keypanel->irq );

	return 0;


l_exit_free_idev:
	input_free_device( input );

l_exit_free_mem:
	kfree( keypanel );
	return rtn;
}


static int __devexit in_keypanel_remove( struct platform_device *pdev )
{
	struct insync_keypanel *keypanel = dev_get_drvdata( &pdev->dev );
	
	spin_lock( &user_lock );
	
//	free_irq( keypanel->irq, keypanel );

	input_unregister_device( keypanel->idev );
	printk( "input device unregistered\n" );
	input_free_device( keypanel->idev );
	printk( "input device removed\n" );

	list_del( &keypanel->list );
	printk( "keypanel list removed\n" );
	kfree( keypanel );
	printk( "keypanel deallocated\n" );

	spin_unlock( &user_lock );

	return 0;
}


static struct platform_driver insync_keypanel_driver = {
	.remove = __devexit_p( in_keypanel_remove ),
	.driver = {
		.name 	= "vkeypanel",
		.owner 	= THIS_MODULE,
	},
};


/*------------------------------------------------------------------------
 * Virtual SSC KeyPanel platform devices for SSC0 and SSC1
 */

static struct platform_device *ssc0_device = NULL;
static struct platform_device *ssc1_device = NULL;

static int in_add_ssc_device(unsigned id )
{
	/*
	 * NOTE: caller is responsible for passing information matching
	 * "pins" to whatever will be using each particular controller.
	 */
	switch (id) {
		case AT91SAM9G45_ID_SSC0: 
			ssc0_device = platform_device_register_simple( "vkeypanel", 0, NULL, 0 );
			break;
		case AT91SAM9G45_ID_SSC1:
			ssc1_device = platform_device_register_simple( "vkeypanel", 1, NULL, 0 );
			break;
	default:
		return -1;
	}

	return 0;
}

static void in_remove_ssc_devices( void )
{
	if ( ssc0_device != NULL )
	{
		platform_device_unregister( ssc0_device );
		printk( "SSC0 device removed\n" );
	}
	if ( ssc1_device != NULL )
	{
		platform_device_unregister( ssc1_device );
		printk( "SSC1 device removed\n" );
	}
}

/*------------------------------------------------------------------------*/


static int __init in_ssc_keypanel_init( void )
{
	int rtn;

	rtn = in_add_ssc_device( AT91SAM9G45_ID_SSC0 );
	if ( rtn )
	{
		printk("in_ssc_keypanel_init: platform_device_register for SSC0 failed\n");
		return rtn;
	}
	printk( "Virtual SSC0 Keypanel device added\n" );


	// set up the sysfs group /sys/devices/platform/insync_keypanel/TX_DATA  
	// for TX data frame input from user space
	rtn = sysfs_create_group( &ssc0_device->dev.kobj, &keypanel_attrg );
	if ( rtn )
	{
		printk("in_ssc_keypanel_init: creating sysfs group for LED data for the TX frame failed\n" );
		platform_device_unregister( ssc0_device );
		return rtn;
	}
	printk( "sysfs group created for TX_DATA\n" );


	
	// set up the sysfs group /sys/devices/platform/insync_keypanel/RX_DATA  
	// for RX data for the simulation
	rtn = sysfs_create_group( &ssc0_device->dev.kobj, &RX_attrg );
	if ( rtn )
	{
		printk("in_ssc_keypanel_init: creating sysfs group for LED data for RX data failed\n" );
		platform_device_unregister( ssc0_device );
		return rtn;
	}
	printk( "[simulation] sysfs group created for RX_DATA\n" );


	rtn = platform_driver_probe( &insync_keypanel_driver, in_keypanel_probe );
	if ( rtn )
	{
		printk("in_ssc_keypanel_init: platfrom_driver_probe failed\n");
		//platform_device_unregister( &at91sam9g45_ssc1_device );
		platform_device_unregister( ssc0_device );
		return rtn;
	}
	printk( "ssc0 device probe() succeeded\n" );

	return rtn;
}

static void __exit in_ssc_keypanel_exit( void )
{
	platform_driver_unregister( &insync_keypanel_driver );
	sysfs_remove_group( &ssc0_device->dev.kobj, &keypanel_attrg );
	printk("sysfs group for TX_DATA removed\n");

	in_remove_ssc_devices();	
}


module_init(in_ssc_keypanel_init);
module_exit(in_ssc_keypanel_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Virtual SSC KeyPanel Driver");
MODULE_AUTHOR("Jeevani W.");

