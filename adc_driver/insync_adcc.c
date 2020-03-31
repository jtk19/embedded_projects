/*
 *  Atmel Touch Screen Driver
 *
 *  Copyright (c) 2008 ATMEL
 *  Copyright (c) 2008 Dan Liang
 *  Copyright (c) 2008 TimeSys Corporation
 *  Copyright (c) 2008 Justin Waters
 *
 *  Based on touchscreen code from Atmel Corporation.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#include <linux/init.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <mach/board.h>
#include <mach/cpu.h>
#include <mach/gpio.h>


/* Register definitions based on AT91SAM9RL64 preliminary draft datasheet */

#define ATMEL_TSADCC_CR		0x00	/* Control register */
#define   ATMEL_TSADCC_SWRST	(1 << 0)	/* Software Reset*/
#define	  ATMEL_TSADCC_START	(1 << 1)	/* Start conversion */

#define ATMEL_TSADCC_MR		0x04	/* Mode register */
#define	  ATMEL_TSADCC_TSAMOD	(3    <<  0)	/* ADC mode */
#define	    ATMEL_TSADCC_TSAMOD_ADC_ONLY_MODE	(0x0)	/* ADC Mode */
#define	    ATMEL_TSADCC_TSAMOD_TS_ONLY_MODE	(0x1)	/* Touch Screen Only Mode */
#define	  ATMEL_TSADCC_LOWRES	(1    <<  4)	/* Resolution selection */
#define	  ATMEL_TSADCC_SLEEP	(1    <<  5)	/* Sleep mode */
#define	  ATMEL_TSADCC_PENDET	(1    <<  6)	/* Pen Detect selection */
#define	  ATMEL_TSADCC_PRES	(1    <<  7)	/* Pressure Measurement Selection */
#define	  ATMEL_TSADCC_PRESCAL	(0x3f <<  8)	/* Prescalar Rate Selection */
#define	  ATMEL_TSADCC_EPRESCAL	(0xff <<  8)	/* Prescalar Rate Selection (Extended) */
#define	  ATMEL_TSADCC_STARTUP	(0x7f << 16)	/* Start Up time */
#define	  ATMEL_TSADCC_SHTIM	(0xf  << 24)	/* Sample & Hold time */
#define	  ATMEL_TSADCC_PENDBC	(0xf  << 28)	/* Pen Detect debouncing time */

#define ATMEL_TSADCC_TRGR	0x08	/* Trigger register */
#define	  ATMEL_TSADCC_TRGMOD	(7      <<  0)	/* Trigger mode */
#define	    ATMEL_TSADCC_TRGMOD_NONE		(0 << 0)
#define     ATMEL_TSADCC_TRGMOD_EXT_RISING	(1 << 0)
#define     ATMEL_TSADCC_TRGMOD_EXT_FALLING	(2 << 0)
#define     ATMEL_TSADCC_TRGMOD_EXT_ANY		(3 << 0)
#define     ATMEL_TSADCC_TRGMOD_PENDET		(4 << 0)
#define     ATMEL_TSADCC_TRGMOD_PERIOD		(5 << 0)
#define     ATMEL_TSADCC_TRGMOD_CONTINUOUS	(6 << 0)
#define   ATMEL_TSADCC_TRGPER	(0xffff << 16)	/* Trigger period */

#define ATMEL_TSADCC_TSR	0x0C	/* Touch Screen register */
#define	  ATMEL_TSADCC_TSFREQ	(0xf <<  0)	/* TS Frequency in Interleaved mode */
#define	  ATMEL_TSADCC_TSSHTIM	(0xf << 24)	/* Sample & Hold time */

#define ATMEL_TSADCC_CHER	0x10	/* Channel Enable register */
#define ATMEL_TSADCC_CHDR	0x14	/* Channel Disable register */
#define ATMEL_TSADCC_CHSR	0x18	/* Channel Status register */
#define	  ATMEL_TSADCC_CH(n)	(1 << (n))	/* Channel number */

#define ATMEL_TSADCC_SR		0x1C	/* Status register */
#define	  ATMEL_TSADCC_EOC(n)	(1 << ((n)+0))	/* End of conversion for channel N */
#define	  ATMEL_TSADCC_OVRE(n)	(1 << ((n)+8))	/* Overrun error for channel N */
#define	  ATMEL_TSADCC_DRDY	(1 << 16)	/* Data Ready */
#define	  ATMEL_TSADCC_GOVRE	(1 << 17)	/* General Overrun Error */
#define	  ATMEL_TSADCC_ENDRX	(1 << 18)	/* End of RX Buffer */
#define	  ATMEL_TSADCC_RXBUFF	(1 << 19)	/* TX Buffer full */
#define	  ATMEL_TSADCC_PENCNT	(1 << 20)	/* Pen contact */
#define	  ATMEL_TSADCC_NOCNT	(1 << 21)	/* No contact */

#define ATMEL_TSADCC_LCDR	0x20	/* Last Converted Data register */
#define	  ATMEL_TSADCC_DATA	(0x3ff << 0)	/* Channel data */

#define ATMEL_TSADCC_IER	0x24	/* Interrupt Enable register */
#define ATMEL_TSADCC_IDR	0x28	/* Interrupt Disable register */
#define ATMEL_TSADCC_IMR	0x2C	/* Interrupt Mask register */
#define ATMEL_TSADCC_CDR0	0x30	/* Channel Data 0 */
#define ATMEL_TSADCC_CDR1	0x34	/* Channel Data 1 */
#define ATMEL_TSADCC_CDR2	0x38	/* Channel Data 2 */
#define ATMEL_TSADCC_CDR3	0x3C	/* Channel Data 3 */
#define ATMEL_TSADCC_CDR4	0x40	/* Channel Data 4 */
#define ATMEL_TSADCC_CDR5	0x44	/* Channel Data 5 */
#define ATMEL_TSADCC_CDR6	0x48	/* Channel Data 6 */
#define ATMEL_TSADCC_CDR7	0x4C	/* Channel Data 7 */


#define ATMEL_TSADCC_XPOS	0x50
#define ATMEL_TSADCC_Z1DAT	0x54
#define ATMEL_TSADCC_Z2DAT	0x58

#define PRESCALER_VAL(x)	((x) >> 8)

#define ADC_DEFAULT_CLOCK	100000


#define	INSYNC_ADC_DATA_MAX		0x1FFF	// 13 bits <= 10 bit reading and 3 bit id


unsigned int adc_channel_mask = 0x00FF;		// default: all 8 channels enabled
module_param( adc_channel_mask, uint, S_IRUGO);

struct insync_adcc {
	struct input_dev	*input;
	struct clk			*clk;
	int					irq;
	unsigned int		absx;
	unsigned int		absy;
	char				phys[32];
};

static void __iomem		*tsc_base;

#define insync_adcc_read(reg)		__raw_readl(tsc_base + (reg))
#define insync_adcc_write(reg, val)	__raw_writel((val), tsc_base + (reg))

static irqreturn_t insync_adcc_interrupt(int irq, void *dev)
{
	struct insync_adcc	*ts_dev = (struct insync_adcc *)dev;
	struct input_dev	*input_dev = ts_dev->input;

	unsigned int evalue = 0;
	unsigned int status;

	status = insync_adcc_read(ATMEL_TSADCC_SR);
	status &= insync_adcc_read(ATMEL_TSADCC_IMR);

	ts_dev->absy = 0;

	if ( status & ATMEL_TSADCC_EOC(0) ) // the only interrupts serviced are EOC
	{
			// always report the current measurement for the channel
			ts_dev->absx = 0;
			ts_dev->absy = insync_adcc_read( ATMEL_TSADCC_CDR0 );
	}
	else if ( status & ATMEL_TSADCC_EOC(1) )
	{
			ts_dev->absx = 1;
			ts_dev->absy = insync_adcc_read( ATMEL_TSADCC_CDR1 );
	}
	else if ( status & ATMEL_TSADCC_EOC(2) )
	{
			ts_dev->absx = 2;
			ts_dev->absy = insync_adcc_read( ATMEL_TSADCC_CDR2 );
	}
	else if ( status & ATMEL_TSADCC_EOC(3) )
	{
			ts_dev->absx = 3;
			ts_dev->absy = insync_adcc_read( ATMEL_TSADCC_CDR3 );
	}
	else if ( status & ATMEL_TSADCC_EOC(4) )
	{
			ts_dev->absx = 4;
			ts_dev->absy = insync_adcc_read( ATMEL_TSADCC_CDR4 );
	}
	else if ( status & ATMEL_TSADCC_EOC(5) )
	{
			ts_dev->absx = 5;
			ts_dev->absy = insync_adcc_read( ATMEL_TSADCC_CDR5 );
	}
	else if ( status & ATMEL_TSADCC_EOC(6) )
	{
			ts_dev->absx = 6;
			ts_dev->absy = insync_adcc_read( ATMEL_TSADCC_CDR6 );
	}
	else if ( status & ATMEL_TSADCC_EOC(7) )
	{
			ts_dev->absx = 7;
			ts_dev->absy = insync_adcc_read( ATMEL_TSADCC_CDR7 );
	}
	else 	// no other interrupts are serviced
	{
		return IRQ_HANDLED;   // Exit here
	}

	// if here, we are still servicing one of the EOC interrupts
	// generate event only if the ADC reading is non-zero
	if ( ts_dev->absy > 0 )
	{
//		input_report_abs( input_dev, ABS_X, ts_dev->absx );
//		input_report_abs( input_dev, ABS_Y, ts_dev->absy );
//		input_report_key( input_dev, BTN_TOUCH, 1);

		// 3 LAB stored the [0-7] channel ID
		// next 10 bits store the ADC reading
		evalue = ( ts_dev->absy << 3 ) | ts_dev->absx;

		input_report_abs( input_dev, ABS_Y, evalue );
		input_sync( input_dev );
	}
 
	return IRQ_HANDLED;
}

/*
 * The functions for inserting/removing us as a module.
 */

static int __devinit insync_adcc_probe(struct platform_device *pdev)
{
	struct insync_adcc	*ts_dev;
	struct input_dev	*input_dev;
	struct resource		*res;
	struct at91_tsadcc_data *pdata = pdev->dev.platform_data;
	int		err = 0, i;
	unsigned int	prsc;
	unsigned int	reg;
	unsigned int	chnl= 0, eoc= 0;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res) {
		dev_err(&pdev->dev, "insync_adcc: no mmio resource defined.\n");
		return -ENXIO;
	}

	/* Allocate memory for device */
	ts_dev = kzalloc(sizeof(struct insync_adcc), GFP_KERNEL);
	if (!ts_dev) {
		dev_err(&pdev->dev, "insync_adcc: failed to allocate memory.\n");
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, ts_dev);

	input_dev = input_allocate_device();
	if (!input_dev) {
		dev_err(&pdev->dev, "insync_adcc: failed to allocate input device.\n");
		err = -EBUSY;
		goto err_free_mem;
	}

	ts_dev->irq = platform_get_irq(pdev, 0);
	if (ts_dev->irq < 0) {
		dev_err(&pdev->dev, "insync_adcc: no irq ID is designated.\n");
		err = -ENODEV;
		goto err_free_dev;
	}

	if (!request_mem_region(res->start, resource_size(res),
				"atmel tsadcc regs")) {
		dev_err(&pdev->dev, "resources is unavailable.\n");
		err = -EBUSY;
		goto err_free_dev;
	}

	tsc_base = ioremap(res->start, resource_size(res));
	if (!tsc_base) {
		dev_err(&pdev->dev, "insync_adcc: failed to map registers.\n");
		err = -ENOMEM;
		goto err_release_mem;
	}

	// register the ISR
	err = request_irq(ts_dev->irq, insync_adcc_interrupt, IRQF_DISABLED,
			pdev->dev.driver->name, ts_dev);
	if (err) {
		dev_err(&pdev->dev, "insync_adcc: failed to allocate irq.\n");
		goto err_unmap_regs;
	}

	ts_dev->clk = clk_get(&pdev->dev, "tsc_clk");
	if (IS_ERR(ts_dev->clk)) {
		dev_err(&pdev->dev, "insync_adcc: failed to get ts_clk\n");
		err = PTR_ERR(ts_dev->clk);
		goto err_free_irq;
	}

	ts_dev->input = input_dev;

	snprintf(ts_dev->phys, sizeof(ts_dev->phys),
		 "%s/input0", dev_name(&pdev->dev));

	input_dev->name = "Insync ADC Controller";
	input_dev->uniq = "insync_adcc";
	input_dev->phys = ts_dev->phys;
	input_dev->dev.parent = &pdev->dev;

	__set_bit(EV_ABS, input_dev->evbit);
	input_set_abs_params(input_dev, ABS_Y, 0, INSYNC_ADC_DATA_MAX, 0, 0);
//	input_set_abs_params(input_dev, ABS_X, 0, 0x3FF, 0, 0); // ABS_X event disabled
//	input_set_capability(input_dev, EV_KEY, BTN_TOUCH); // /dev/input/mouseX interface diabled

	/* clk_enable() always returns 0, no need to check it */
	clk_enable(ts_dev->clk);

	prsc = clk_get_rate(ts_dev->clk);
	dev_info(&pdev->dev, "Master clock is set at: %d Hz\n", prsc);

	if (!pdata)
		goto err_fail;

	if (!pdata->adc_clock)
		pdata->adc_clock = ADC_DEFAULT_CLOCK;

	prsc = (prsc / (2 * pdata->adc_clock)) - 1;

	/* saturate if this value is too high */
	if (cpu_is_at91sam9rl()) {
		if (prsc > PRESCALER_VAL(ATMEL_TSADCC_PRESCAL))
			prsc = PRESCALER_VAL(ATMEL_TSADCC_PRESCAL);
	} else {
		if (prsc > PRESCALER_VAL(ATMEL_TSADCC_EPRESCAL))
			prsc = PRESCALER_VAL(ATMEL_TSADCC_EPRESCAL);
	}

	dev_info(&pdev->dev, "Prescaler is set at: %d\n", prsc);
	dev_info(&pdev->dev, "i.e. ADCCLK is set to: %d\n", pdata->adc_clock );


	insync_adcc_write(ATMEL_TSADCC_CR, ATMEL_TSADCC_SWRST);


	reg = ATMEL_TSADCC_TSAMOD_ADC_ONLY_MODE |
		((0x00 << 4) & ATMEL_TSADCC_LOWRES) |   // high (10 bit) resolution conversions
		((0x00 << 5) & ATMEL_TSADCC_SLEEP)	|	// Normal Mode 
//		((0x01 << 6) & ATMEL_TSADCC_PENDET)	|	// Enable Pen Detect 
		(prsc << 8)				|
		((0x26 << 16) & ATMEL_TSADCC_STARTUP)	|
		((pdata->ts_sample_hold_time << 24) & ATMEL_TSADCC_SHTIM )  |
		((pdata->pendet_debounce << 28) & ATMEL_TSADCC_PENDBC);
	insync_adcc_write(ATMEL_TSADCC_MR, reg);

	// conversion triggered by the rising edge on the TSADTRIG pin; ie. TSADTRIG 
	// pin must be connected, and conversion is triggered when it goes low->high.  
	// Change lines below if trigger is by TSADTRG going from high to low.
	//insync_adcc_write(ATMEL_TSADCC_TRGR, ATMEL_TSADCC_TRGMOD_EXT_FALLING );
	insync_adcc_write(ATMEL_TSADCC_TRGR, ATMEL_TSADCC_TRGMOD_EXT_RISING );

	// conversion is performed on a full range between 0V and 
	// the reference voltage pin TSADVREF.

	// set all 8 channels 0 - 7
	for ( i = 0; i<7; ++i )
	{
		chnl |= ATMEL_TSADCC_CH(i);
		eoc  |= ATMEL_TSADCC_EOC(i);
	}
	// now enable only those set by the mask (module parameter) 
	chnl &= adc_channel_mask;
	eoc  &= adc_channel_mask;

	insync_adcc_write( ATMEL_TSADCC_CHER, chnl );

	// enable only the EOC interrupt after each conversion is complete
	insync_adcc_write( ATMEL_TSADCC_IER, eoc );

	insync_adcc_read(ATMEL_TSADCC_SR);

	/* All went ok, so register the input system */
	err = input_register_device(input_dev);
	if (err)
		goto err_fail;

	return 0;

err_fail:
	clk_disable(ts_dev->clk);
	clk_put(ts_dev->clk);
err_free_irq:
	free_irq(ts_dev->irq, ts_dev);
err_unmap_regs:
	iounmap(tsc_base);
err_release_mem:
	release_mem_region(res->start, resource_size(res));
err_free_dev:
	input_free_device(input_dev);
err_free_mem:
	kfree(ts_dev);
	return err;
}

static int __devexit insync_adcc_remove(struct platform_device *pdev)
{
	struct insync_adcc *ts_dev = dev_get_drvdata(&pdev->dev);
	struct resource *res;

	clk_disable(ts_dev->clk);
	clk_put(ts_dev->clk);

	free_irq(ts_dev->irq, ts_dev);

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	iounmap(tsc_base);
	release_mem_region(res->start, resource_size(res));

	input_unregister_device(ts_dev->input);
	input_free_device( ts_dev->input );

	kfree(ts_dev);

	return 0;
}

static struct platform_driver insync_adcc_driver = {
	.probe		= insync_adcc_probe,
	.remove		= __devexit_p(insync_adcc_remove),
	.driver		= {
		.name	= "insync_adcc",
	},
};


/*----------------------------------------------------------------------
 * Insync ADCC platform device
 */

static u64 tsadcc_dmamask = DMA_BIT_MASK(32);
static struct at91_tsadcc_data at91sam9g45_tsadcc_data = {
	.adc_clock		= 300000,
	.pendet_debounce	= 0x0d,
	.ts_sample_hold_time	= 0x0a,
};


static struct resource tsadcc_resources[] = {
	[0] = {
		.start	= AT91SAM9G45_BASE_TSC,
		.end	= AT91SAM9G45_BASE_TSC + SZ_16K - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= AT91SAM9G45_ID_TSC,
		.end	= AT91SAM9G45_ID_TSC,
		.flags	= IORESOURCE_IRQ,
	}
};

static struct platform_device at91sam9g45_tsadcc_device = {
	.name		= "insync_adcc",
	.id		= -1,
	.dev		= {
				.dma_mask		= &tsadcc_dmamask,
				.coherent_dma_mask	= DMA_BIT_MASK(32),
				.platform_data		= &at91sam9g45_tsadcc_data,
	},
	.resource	= tsadcc_resources,
	.num_resources	= ARRAY_SIZE(tsadcc_resources),
};

void __init at91_add_device_tsadcc(struct at91_tsadcc_data *data)
{
	if ( !data )
		return;

	at91_set_gpio_input(AT91_PIN_PD20, 0);	// AD0_XR 
	at91_set_gpio_input(AT91_PIN_PD21, 0);	// AD1_XL
	at91_set_gpio_input(AT91_PIN_PD22, 0);	// AD2_YT
	at91_set_gpio_input(AT91_PIN_PD23, 0);	// AD3_TB

	// configure the 4 general purpose analogue input channel pins
	at91_set_gpio_input(AT91_PIN_PD24, 0);	// GPAD4
	at91_set_gpio_input(AT91_PIN_PD25, 0);	// GPAD5
	at91_set_gpio_input(AT91_PIN_PD26, 0);	// GPAD6
	at91_set_gpio_input(AT91_PIN_PD27, 0);	// GPAD7

	// configure the TSADTRG external trigger interrupt
	at91_set_A_periph( AT91_PIN_PD28, 0 );

	platform_device_register( &at91sam9g45_tsadcc_device );
}

void remove_device_tsadcc( void )
{
	platform_device_unregister( &at91sam9g45_tsadcc_device );
}

/*----------------------------------------------------------------------*/


static int __init insync_adcc_init(void)
{
	int rtn;

	at91_add_device_tsadcc( &at91sam9g45_tsadcc_data );
	
	rtn = platform_driver_register(&insync_adcc_driver);
	if ( rtn )
	{
		printk("insync_adcc: platform_driver_register() failed\n" );
		remove_device_tsadcc();
	}

	return rtn;
}

static void __exit insync_adcc_exit(void)
{
	platform_driver_unregister(&insync_adcc_driver);

	remove_device_tsadcc();
}

module_init(insync_adcc_init);
module_exit(insync_adcc_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Insync ADC Controller Driver");
MODULE_AUTHOR("Jeevani W.");

