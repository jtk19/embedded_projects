#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/input.h>
#include <linux/clk.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <mach/board.h>
#include <mach/gpio.h>

#include "insync-ssc.h"
#include "insync_keypanel.h"


/* uncomment line below for production; comment out for testing & debugging */
#define NDEBUG


//	only data and frame-sync pins enabled; no output TK/RK clock signals 
//	enabled since TX/RX pads are clocked off the MCK clock divider
#define IN_SSC_TX_RX_CONFIG	( ATMEL_SSC_TF | ATMEL_SSC_TD | ATMEL_SSC_RF | ATMEL_SSC_RD )

#define IN_SSC_MAX_CLK_DIV			4095		// min speed = MCK / (2*4095)	
#define IN_SSC_DESIRED_CLK_SPEED_HZ	10000000	// about 10 MHz preferred


// masks for the interrupts we enable
static const unsigned int IN_SSC_ENDRX_MASK = ( 1 << SSC_IER_ENDRX_OFFSET );
static const unsigned int IN_SSC_OVRUN_MASK = ( 1 << SSC_IER_OVRUN_OFFSET );
#ifndef NDEBUG
static const unsigned int IN_SSC_ENDTX_MASK = ( 1 << SSC_IER_ENDTX_OFFSET );
#endif


// for serialized access to the SSC
static DEFINE_SPINLOCK( user_lock );
static LIST_HEAD( ssc_list );


// main device structure
struct insync_keypanel
{
	struct list_head	list;
	int					user;

	struct platform_device 	*pdev;
	struct clk			*clk;
	int					 irq;
	void __iomem		*regs;

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

// device clocked from init to remove
//	clk_enable(ssc->clk);

	return ssc;
}
//EXPORT_SYMBOL(in_ssc_request);


void in_ssc_free(struct insync_keypanel *ssc)
{
	spin_lock(&user_lock);
	if (ssc->user) {
		ssc->user--;
//		clk_disable(ssc->clk);
	} else {
		dev_dbg(&ssc->pdev->dev, "device already free\n");
	}
	spin_unlock(&user_lock);
}
//EXPORT_SYMBOL(in_ssc_free);


/*------------------------------------------------------------------------
 * The ISR & supporting functions
 */

static inline void process_RX_data_frame( struct insync_keypanel *keypanel )
{
	unsigned int key_states, prev_key_states;
//	unsigned int prev_key_state, prev_key_state;
	unsigned int encoder;
	unsigned int xor, mask;
	unsigned int evalue = 0;		// event ABS_Y value
	unsigned short key_id = 0, max;

	keypanel->prev_rx_frame = keypanel->rx_frame;
	keypanel->rx_frame = ssc_readl( keypanel->regs, RHR );	// read the data frame from the Receive Hold Register

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

	// 80% of the time the key events are from the alpha-numeric keys - id's [0,9]
	// and most frames are likeyly to carry a single key event each, though multiple
	// simultaneous key-presses are allowed and processed.
	// Narrow down the search for optimization whereever possible.
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
static irqreturn_t insync_keypanel_interrupt(int irq, void *dev)
{
	struct insync_keypanel 	*keypanel;
	unsigned int status;


	spin_lock( &user_lock );

	keypanel = (struct insync_keypanel *)dev;

	status = ssc_readl( keypanel->regs, SR );
	status &= ssc_readl( keypanel->regs, IMR );

	if ( status & IN_SSC_ENDRX_MASK )	// we have just received an RX data frame
	{
		process_RX_data_frame( keypanel );
	}
	else if ( status & IN_SSC_OVRUN_MASK )
	{
		printk( "One RX data frame lost.\n" );
	}
#ifndef NDEBUG
	else if ( status & IN_SSC_ENDTX_MASK )
	{
		printk( "One TX frame transmitted.\n" );
	}
#endif
	// no other interrupts are serviced

	spin_unlock( &user_lock );

	return IRQ_HANDLED;
}


/*------------------------------------------------------------------------
 * The sysfs group for the input of the LED driving TX frame
 */

static ssize_t write_LED_dataframe( struct device *dev,
									struct device_attribute *attr,
									const char *buffer, size_t count )
{
	struct insync_keypanel *keypanel = NULL;
	unsigned int LED_states;
	int rtn, tries =  0;
	

	rtn = sscanf( buffer, "%x", &LED_states );
	if ( rtn != 1 ) // some error
	{
		printk( "LED data error in [%s]\n", buffer );
		return -1;
	}

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
			in_ssc_free( keypanel );
			return -1;
		}
		if 	( keypanel == ERR_PTR(-ENODEV)  ||  keypanel == ERR_PTR(-EBUSY) )
		{
			mdelay( 1 ); // 1 millisecond wait
		}
	}

	ssc_writel( keypanel->regs, THR, LED_states );

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
 * Insync SSC KeyPanel platform driver
 */

static int in_keypanel_probe( struct platform_device *pdev )
{
	struct resource 			*res;
	struct insync_keypanel		*keypanel;
	struct input_dev			*input;
	unsigned int	mck, clk_div;
	unsigned int	reg;
	int	rtn = 0;
	
				
	// allocate memory for our device
	keypanel = kzalloc( sizeof( struct insync_keypanel ), GFP_KERNEL );
	if ( !keypanel )
	{
		dev_err( &pdev->dev, "probe(): memory allocation for insync_keypanel failed\n");
		return -ENOMEM;
	}
	keypanel->pdev = pdev;


	// allocate input group device
	input = input_allocate_device();
	if ( !input )
	{
		dev_err( &pdev->dev, "probe(): failed to allocate input device\n");
		rtn = -EBUSY;
		goto l_exit_free_mem;
	}
	keypanel->idev = input;


	// allocate and reserve IO memory resources
	res = platform_get_resource( pdev, IORESOURCE_MEM, 0 );
	if ( !res )
	{
		dev_err( &pdev->dev, "probe(): failed to allocate resource memory\n");
		rtn = -ENXIO;
		goto l_exit_free_idev;
	}

	if (!request_mem_region(res->start, resource_size(res), "insync keypanel regs")) 
	{
		dev_err(&pdev->dev, "probe(): resources is unavailable.\n");
		rtn = -EBUSY;
		goto l_exit_free_idev;
	}

	keypanel->regs = ioremap(res->start, resource_size(res));
	if ( !keypanel->regs ) 
	{
		dev_err(&pdev->dev, "probe(): failed to map registers.\n");
		rtn = -ENOMEM;
		goto l_exit_release_mem;
	}

	
	// set up the interrupts 
	keypanel->irq = platform_get_irq(pdev, 0);
	if ( keypanel->irq < 0 ) 
	{
		dev_err( &pdev->dev, "probe(): no irq ID is designated.\n");
		rtn = -ENODEV;
		goto l_exit_iounmap;
	}

	// register the ISR
	rtn = request_irq( keypanel->irq, insync_keypanel_interrupt, IRQF_DISABLED,
			pdev->dev.driver->name, keypanel);
	if ( rtn ) 
	{
		dev_err( &pdev->dev, "insync_adcc: failed to allocate irq.\n");
		goto l_exit_iounmap;
	}
 

	// set up the clock
	keypanel->clk = clk_get( &pdev->dev, "pclk");
	if ( IS_ERR( keypanel->clk ) ) 
	{
		dev_err( &pdev->dev, "probe(): no pclk clock defined\n");
		rtn = -ENXIO;
		goto l_exit_free_irq;
	}


	// initialise the keypanel device and configure the input device
	keypanel->rx_frame = 0x00000000;
	keypanel->prev_rx_frame = 0x00000000;
	snprintf( keypanel->phys, sizeof( keypanel->phys ), 
			"%s/input0", dev_name(&pdev->dev) );
	
	input->name = "Insync SSC KeyPanel Controller";
	input->uniq = "insync_keypanel";
	input->phys = keypanel->phys;
	input->dev.parent = &pdev->dev;

	__set_bit( EV_ABS, input->evbit );
	input_set_abs_params( input, ABS_Y, 0, 0xFFFF, 0, 0); // value sent up is 16 bits


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

	/*  configure RX  */
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


	/*  configure TX  */
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


	/*  configure the interrupts  */
	reg = 0x00000000 |
			( 0x1 	<< SSC_IER_ENDRX_OFFSET ) |		// enable end-of-reception interrupt for the frame
			( 0x1 	<< SSC_IER_OVRUN_OFFSET ) ;		// alert us if RX data is overwritten before we can read

#ifndef NDEBUG
	reg |=	( 0x1	<< SSC_IER_ENDTX_OFFSET ) ;		// success alert at end of transmission
											// only for debugging purposes, diable for production
#endif
	
	ssc_writel( keypanel->regs, IER, reg );		// enable the above interrutps

	ssc_readl( keypanel->regs, SR );

	/* now set to continuous receipt on RX ; so Compare 0 is not used. */
	//enable_frame_recieve( keypanel ); // start off the RX by writing a new Compare 0

	platform_set_drvdata( pdev, keypanel);

	// all went ok, so register the input subsystem
	rtn = input_register_device(input);
	if (rtn)
	{
		dev_err( &pdev->dev, "probe(): input subsystem failed to register\n");
		goto l_err_fail;
	}

	spin_lock( &user_lock );
	list_add_tail( &keypanel->list, &ssc_list);
	spin_unlock( &user_lock );

	dev_info( &pdev->dev, "Insync SSC KeyPanel at 0x%p (irq %d)\n",
				keypanel->regs, keypanel->irq );

	return 0;



l_err_fail:
	clk_disable( keypanel->clk );
	clk_put( keypanel->clk );

l_exit_free_irq:
	free_irq( keypanel->irq, keypanel );

l_exit_iounmap:
	iounmap( keypanel->regs );

l_exit_release_mem:
	release_mem_region( res->start, resource_size(res) );

l_exit_free_idev:
	input_free_device( input );

l_exit_free_mem:
	kfree( keypanel );
	return rtn;
}


static int __devexit in_keypanel_remove( struct platform_device *pdev )
{
	struct insync_keypanel *keypanel = dev_get_drvdata( &pdev->dev );
	struct resource *res;
	
	spin_lock( &user_lock );
	
	clk_disable( keypanel->clk );
	clk_put( keypanel->clk );

	free_irq( keypanel->irq, keypanel );

	iounmap( keypanel->regs );
	res = platform_get_resource( pdev, IORESOURCE_MEM, 0 );
	if ( res )
	{
		release_mem_region( res->start, resource_size(res) );
	}

	input_unregister_device( keypanel->idev );
	input_free_device( keypanel->idev );

	list_del( &keypanel->list );
	kfree( keypanel );

	spin_unlock( &user_lock );

	return 0;
}


static struct platform_driver insync_keypanel_driver = {
	.remove = __devexit_p( in_keypanel_remove ),
	.driver = {
		.name 	= "insync_keypanel",
		.owner 	= THIS_MODULE,
	},
};


/*------------------------------------------------------------------------
 * Insync SSC KeyPanel platform devices for SSC0 and SSC1
 */

static u64 ssc0_dmamask = DMA_BIT_MASK(32);

static struct resource ssc0_resources[] = {
	[0] = {
		.start	= AT91SAM9G45_BASE_SSC0,
		.end	= AT91SAM9G45_BASE_SSC0 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9G45_ID_SSC0,
		.end	= AT91SAM9G45_ID_SSC0,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9g45_ssc0_device = {
	.name	= "insync_keypanel",
	.id	= 0,
	.dev	= {
		.dma_mask		= &ssc0_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.resource	= ssc0_resources,
	.num_resources	= ARRAY_SIZE(ssc0_resources),
};

static inline void configure_ssc0_pins(unsigned pins)
{
	if (pins & ATMEL_SSC_TF)
		at91_set_A_periph(AT91_PIN_PD1, 1);
	if (pins & ATMEL_SSC_TK)
		at91_set_A_periph(AT91_PIN_PD0, 1);
	if (pins & ATMEL_SSC_TD)
		at91_set_A_periph(AT91_PIN_PD2, 1);
	if (pins & ATMEL_SSC_RD)
		at91_set_A_periph(AT91_PIN_PD3, 1);
	if (pins & ATMEL_SSC_RK)
		at91_set_A_periph(AT91_PIN_PD4, 1);
	if (pins & ATMEL_SSC_RF)
		at91_set_A_periph(AT91_PIN_PD5, 1);
}

static u64 ssc1_dmamask = DMA_BIT_MASK(32);

static struct resource ssc1_resources[] = {
	[0] = {
		.start	= AT91SAM9G45_BASE_SSC1,
		.end	= AT91SAM9G45_BASE_SSC1 + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9G45_ID_SSC1,
		.end	= AT91SAM9G45_ID_SSC1,
		.flags	= IORESOURCE_IRQ,
	},
};

static struct platform_device at91sam9g45_ssc1_device = {
	.name	= "insync_keypanel",
	.id	= 1,
	.dev	= {
		.dma_mask		= &ssc1_dmamask,
		.coherent_dma_mask	= DMA_BIT_MASK(32),
	},
	.resource	= ssc1_resources,
	.num_resources	= ARRAY_SIZE(ssc1_resources),
};

static inline void configure_ssc1_pins(unsigned pins)
{
	if (pins & ATMEL_SSC_TF)
		at91_set_A_periph(AT91_PIN_PD14, 1);
	if (pins & ATMEL_SSC_TK)
		at91_set_A_periph(AT91_PIN_PD12, 1);
	if (pins & ATMEL_SSC_TD)
		at91_set_A_periph(AT91_PIN_PD10, 1);
	if (pins & ATMEL_SSC_RD)
		at91_set_A_periph(AT91_PIN_PD11, 1);
	if (pins & ATMEL_SSC_RK)
		at91_set_A_periph(AT91_PIN_PD13, 1);
	if (pins & ATMEL_SSC_RF)
		at91_set_A_periph(AT91_PIN_PD15, 1);
}


static int in_add_ssc_device(unsigned id, unsigned pins)
{
	struct platform_device *pdev;

	/*
	 * NOTE: caller is responsible for passing information matching
	 * "pins" to whatever will be using each particular controller.
	 */
	switch (id) {
	case AT91SAM9G45_ID_SSC0:
		pdev = &at91sam9g45_ssc0_device;
		configure_ssc0_pins(pins);
		break;
	case AT91SAM9G45_ID_SSC1:
		pdev = &at91sam9g45_ssc1_device;
		configure_ssc1_pins(pins);
		break;
	default:
		return -1;
	}

	return platform_device_register(pdev);
}

static void in_remove_ssc_devices( void )
{
	// ssc1 is not yet used
	// platform_device_unregister(  &at91sam9g45_ssc1_device );
	platform_device_unregister(  &at91sam9g45_ssc0_device );
}

/*------------------------------------------------------------------------*/


static int __init in_ssc_keypanel_init( void )
{
	int rtn;

	rtn = in_add_ssc_device( AT91SAM9G45_ID_SSC0, IN_SSC_TX_RX_CONFIG );
	if ( rtn )
	{
		printk("in_ssc_keypanel_init: platform_device_register for SSC0 failed\n");
		return rtn;
	}

	// set up the sysfs group /sys/devices/platform/insync_keypanel/TX_DATA  
	// for TX data frame input from user space
	rtn = sysfs_create_group( &at91sam9g45_ssc0_device.dev.kobj, &keypanel_attrg );
	if ( rtn )
	{
		printk("in_ssc_keypanel_init: creating sysfs group for LED data for the TX frame failed\n" );
		//platform_device_unregister( &at91sam9g45_ssc1_device );
		platform_device_unregister( &at91sam9g45_ssc0_device );
		return rtn;
	}

	// ssc1 is not yet used
	/*
	in_add_ssc_device( AT91SAM9G45_ID_SSC1, IN_SSC_TX_RX_CONFIG );
	if ( rtn )
	{
		printk("in_ssc_keypanel_init: platform_device_register for SSC0 failed\n");
		platform_device_unregister( &at91sam9g45_ssc0_device );
		return rtn;
	}
	*/

	rtn = platform_driver_probe( &insync_keypanel_driver, in_keypanel_probe );
	if ( rtn )
	{
		printk("in_ssc_keypanel_init: platfrom_driver_probe failed\n");
		//platform_device_unregister( &at91sam9g45_ssc1_device );
		platform_device_unregister( &at91sam9g45_ssc0_device );
		return rtn;
	}

	return rtn;
}

static void __exit in_ssc_keypanel_exit( void )
{
	platform_driver_unregister( &insync_keypanel_driver );

	sysfs_remove_group( &at91sam9g45_ssc0_device.dev.kobj, &keypanel_attrg );

	in_remove_ssc_devices();	
}


module_init(in_ssc_keypanel_init);
module_exit(in_ssc_keypanel_exit);


MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Insync SSC KeyPanel Driver");
MODULE_AUTHOR("Jeevani W.");

