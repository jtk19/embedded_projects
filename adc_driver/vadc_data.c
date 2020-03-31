/*----------------------------------------------------------------------------------
 * Name: 		vadc_data.c
 * Author:		Taxila
 * Date:		August 2011
 * Copyright: 	InSync Technology Ltd
 * Description:	A test tool that generates test data for testing the Controller
 * 				and other user space programs using vadcc, the virtual simulator
 * 				of the Insync ADC multiplexer kernel loadable module.
 *				Ensure that the module vadcc.ko is correctly loaded before running.
 *----------------------------------------------------------------------------------*/

#include <stdio.h>
#include <fcntl.h>
#include <math.h>
#include <string.h>
#include <time.h>


const char *REGISTER_SIM_FILE = "/sys/devices/platform/vadcc/ADCC_CDR";
const char *usage = "usage: vadc_data <s 1-60>\n\t generate ADC data every s seconds\n"; 

int main( int argc, char *argv[] )
{
	const unsigned int MAX_ADC_READING = pow(2,10) - 1; // 10 bit data

	int sim_reg;
	int x = 0, y = 77, sec = 1;
	char buffer[16];
	
	const time_t tnow = time( NULL );
	struct tm *ptnow = localtime( &tnow );

	// open the sysfs file that simulates the ADC 
	// Channel Data registers
	sim_reg = open( REGISTER_SIM_FILE, O_RDWR );
	if ( sim_reg < 0 )
	{
		printf( "Couldn't open vadcc CDR Register file: %s\nload module vadcc.ko", 
					REGISTER_SIM_FILE );
		return -1;
	}

	if ( argc >= 2 )
	{
		sec = atoi( argv[1] );
		if ( sec < 1  || sec > 60 )
		{
			printf( "[%d] %s", sec, usage);
			sec = 1;
		}
	}
	else
	{
		printf( "%s", usage );
	}
	printf( "generating ADC data every %d second(s)\n", sec );

	y = ptnow->tm_sec; // seed random() with seconds after the hour
	while (1)
	{
		srandom( y );
		x = random() % 8;					// range 0 - 7
		y = random() % (MAX_ADC_READING+1); // range 0 - 1023
		sprintf( buffer, "%d,%d\n", x, y );
		printf( buffer, "ADC Reading: %d, %d\n", x, y );
		write( sim_reg, buffer, strlen(buffer) );
		fsync( sim_reg );

		sleep( sec );
	}

	return 0;
}
