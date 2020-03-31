/*-----------------------------------------------------------------------------
 * Name: 		adc_test.c
 * Author:		Taxila
 * Date:		September 2011
 * Copyright: 	InSync Technology Ltd
 * Description:	A Test program to test the Insync ADC multiplexer and the
 * 				virtual ADC controller simulator.  To be used as a template
 * 				for the Controller to use the ADC Data.
 * 				Run without an argument to test insync_adcc.
 * 				Run with a "-v" switch to use with the virtual vadcc.
 *				Ensure that insync_adcc.ko or vadcc.ko is loaded into the 
 *				kernel prior to running.
 *-----------------------------------------------------------------------------*/
#include <stdio.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <linux/input.h>

#define	ADC_ID_MASK			0x0007
#define	ADC_READING_MASK	0x1FF8


static const char *IN_ADC_EVENT_DEVICE_VIRT = "/dev/input/by-path/platform-vadcc-event";
static const char *IN_ADC_EVENT_DEVICE = "/dev/input/by-path/platform-insync_adcc-event";


static struct sigaction intrHandler;

static int	adc_event_fd;
static char adc_devname[256];



static void cleanup()
{
	sigemptyset( &intrHandler.sa_mask );
	close( adc_event_fd );
}

static void iHandler( int s )
{
	printf( "exiting ... on signal [%d]\n", s );
	cleanup();
	exit (s);
}

static void error_exit( char *err, int erri )
{
	perror( err );
	iHandler( erri );
}

static int init( const char *dev )
{
	int rtn;

	// catch and process all these signals for a graceful shutdown
	intrHandler.sa_handler 	= iHandler;
	intrHandler.sa_flags	= 0;
	sigemptyset( &intrHandler.sa_mask );
	sigaction( SIGABRT, &intrHandler, NULL );
	sigaction( SIGHUP, &intrHandler, NULL );
	sigaction( SIGINT, &intrHandler, NULL );
	sigaction( SIGQUIT, &intrHandler, NULL );
	sigaction( SIGTERM, &intrHandler, NULL );
	sigaction( SIGTSTP, &intrHandler, NULL );
	sigaction( SIGUSR1, &intrHandler, NULL );
	sigaction( SIGUSR2, &intrHandler, NULL );

	// initialize the adcc event interface
	adc_event_fd = open( dev, O_RDONLY );
	if ( adc_event_fd == -1 )
	{
		printf( "init(): failed to open ADC event device %s\n", IN_ADC_EVENT_DEVICE );
		return -1;
	}

	ioctl( adc_event_fd, EVIOCGNAME( sizeof(adc_devname) ), adc_devname );
	printf( "init(): ADC event device %s (%s) initialized\n", dev, adc_devname );
	
	return 0;
}


static void adc_process_reading( int adc_id, int adc_reading )
{
	// do whatever you need with the ADC reading here
	printf( "processing ADC reading for ADC %d: %d\n", adc_id, adc_reading );
}


int main( int argc, char *argv[] )
{
	struct input_event adc_ev[2];
	const char **devp;
	int size = sizeof( struct input_event ) *2 ;
	int adc_id = -1, adc_reading = -1, i = 0;
	int rd;
	int rtn;


	devp = &IN_ADC_EVENT_DEVICE;
	if ( argc > 1  &&   strcmp( argv[1], "-v" ) == 0 )
	{
		devp = &IN_ADC_EVENT_DEVICE_VIRT;
	}
	printf( "Device: %s\n", *devp );


	rtn = init( *devp );
	if ( rtn )
	{
		printf( "exiting - initialization failed, errno %d\n", errno );
		cleanup();
		return -1;
	}


	while ( 1 )
	{
		rd = read( adc_event_fd, &adc_ev, size );
		if ( rd < 0 )
		{
			printf( "main(): adc event read failed, errno %d\n", errno );
			error_exit( "read()", rd );
		}
		printf( "ADC ABS_Y event: type = %d, code = %d, value = %d\n",
					adc_ev[0].type, adc_ev[0].code, adc_ev[0].value );
		printf( "ADC SYNC event: type = %d, code = %d, value = %d\n",
					adc_ev[1].type, adc_ev[1].code, adc_ev[1].value );

		// sanity checks
		i = -1;
		if ( adc_ev[0].type == EV_ABS   &&  adc_ev[0].code == ABS_Y )	
		{
			i = 0;		// as expected, the first event holds the ADC data
		}
		else if ( adc_ev[1].type == EV_ABS  &&  adc_ev[1].code == ABS_Y )	
		{
			i = 1;		// some mix-up, the second event holds the ADC data
		}
		//else error, neither event holds ADC data : i = -1
		
		if ( i >= 0 )
		{
			adc_id		= adc_ev[i].value & ADC_ID_MASK;
			adc_reading = ( adc_ev[i].value & ADC_READING_MASK ) >> 3;
			adc_process_reading( adc_id, adc_reading );
		}
		printf ("--------------------------\n" );

	}
	
	return 0;
}
