/*-----------------------------------------------------------------------------
 * Name: 		vadcc.c
 * Author:		Taxila
 * Date:		August 2011
 * Copyright: 	InSync Technology Ltd
 * Description:	A virtual simulator that exactly simulates the Insync ADC 
 * 				multiplexer into the user space.
 * 				Please ensure that the kernel is built with drivers/input/evdev
 * 				incorporated prior to loading this module.
 *-----------------------------------------------------------------------------*/

#include <linux/module.h>
#include <linux/fs.h>
#include <asm/uaccess.h>
#include <linux/pci.h>
#include <linux/input.h>
#include <linux/platform_device.h>

// device families

#define ADC_DATA_MAX	0x1FFF		// 13 bits <= 10 bit reading and 3 bit id	


unsigned int adc_channel_mask = 0xFF;
module_param(adc_channel_mask, uint, S_IRUGO);

static struct platform_device 	*pdev;
static struct input_dev 		*idev;

char this_phys[32];


/*----------------------------------------------------------------
 * sysfs info
 */

static ssize_t write_vadcc( struct device *dev,
						 	struct device_attribute *attr,
						 	const char *buffer, size_t count )
{
	int x = 0, y = 0;

	sscanf( buffer, "%d,%d", &x, &y );
	printk("processing [ %d, %d ]\n", x, y );

	// left shift the 10-bit reading by 3 bits and store 
	// the ADC id in the least significant 3 bits
	y = ( y << 3 ) | x;		

	if ( y > 0  &&  x >= 0  &&  x < 8  &&  y < ADC_DATA_MAX )
	{
//		input_report_abs( idev, ABS_X, x );
		input_report_abs( idev, ABS_Y, y );
//		input_report_key( idev, BTN_TOUCH, 1 );
		input_sync( idev );
		printk("tsadcc events sent\n");
	}
	else
	{
		printk("no event sent: data range error\n");
	}

	return count;
}


DEVICE_ATTR( ADCC_CDR, 0666, NULL, write_vadcc );

static struct attribute *vadcc_attrs[] = {
	&dev_attr_ADCC_CDR.attr,
	NULL
};

static struct attribute_group vadcc_attrg = 
{
	.attrs = vadcc_attrs,
};

/*----------------------------------------------------------------*/

void vadcc_cleanup(void);

static int __init vadcc_init(void)
{
	int rtn;

	pdev = platform_device_register_simple( "vadcc", -1, NULL, 0 );
	if ( IS_ERR(pdev) )
	{
		printk("vadcc device registering failed\n" );
		return PTR_ERR(pdev);
	}
	printk("device registered: %s\n", dev_name(&pdev->dev) );

	rtn = sysfs_create_group( &pdev->dev.kobj, &vadcc_attrg );
	if ( rtn )
	{
		printk("vadcc: sysfs_create_group failed\n" );
		platform_device_unregister(pdev); 
		return rtn;
	}

	rtn = alloc_chrdev_region( &pdev->dev.devt, 0, 1, "vadcc" );
	if ( rtn )
	{
		printk("vadcc device major/minor allocation failed\n" );
	}
	else
	{
		printk("vadcc device: [ %d, %d ]\n", MAJOR(pdev->dev.devt), 
										   MINOR(pdev->dev.devt) );
	}

	idev = input_allocate_device();
	if ( !idev )
	{
		printk("vadcc: input_allocate_device failed\n");
		unregister_chrdev_region( pdev->dev.devt, 1 );
		sysfs_remove_group( &pdev->dev.kobj, &vadcc_attrg );
		platform_device_unregister(pdev); 
		return PTR_ERR(idev);
	}
	printk( "input device allocated\n" );

	snprintf( this_phys, sizeof( this_phys), "%s/input0", dev_name(&pdev->dev));

	idev->name = "Virtual ADC Controller";
	idev->uniq = "vadcc";
	idev->phys = this_phys;
	idev->dev.parent = &pdev->dev;

	set_bit( EV_ABS, idev->evbit );	
//	input_set_abs_params( idev, ABS_X, 0, 7, 0, 0);
	input_set_abs_params( idev, ABS_Y, 0, ADC_DATA_MAX, 0, 0);
//	input_set_capability( idev, EV_KEY, BTN_TOUCH );

	rtn = input_register_device( idev );
	if ( rtn )
	{
		printk("vadcc: input_register_device failed\n" );
		vadcc_cleanup();
		return rtn;
	}

	printk("jw vadcc initialized for with channel mask [0x%0X]\n", adc_channel_mask); 

	return 0;
}

void vadcc_cleanup(void)
{
	input_unregister_device( idev );
	input_free_device( idev );

	unregister_chrdev_region( pdev->dev.devt, 1 );
	sysfs_remove_group( &pdev->dev.kobj, &vadcc_attrg );

	platform_device_unregister(pdev); 

	printk("jw vadcc mouse exit\n");
	return;
}

module_init(vadcc_init);
module_exit(vadcc_cleanup);

MODULE_LICENSE("GPL");
MODULE_ALIAS("virt_adcc");
