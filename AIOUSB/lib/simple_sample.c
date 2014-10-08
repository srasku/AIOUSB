#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <aiousb.h>
#include "ADCConfigBlock.h"
#include "AIOContinuousBuffer.h"

/**
 * @brief the type of configuration from the command line that we accept
 */
typedef enum {
    ADCCONIGBLOCK_CONFIG = 0,
    AIOCONTINUOUSBUF_CONFIG = 1
} ConfigurationType;

/**
 * @brief 
 */
typedef union  aio_setting {
    ADCConfigBlock adc;
    AIOContinuousBuf aiobuf;
} AIOSetting;

typedef struct configuration {
    ConfigurationType type;
    int timeout;
    int buffer_size;
    int discard_first_sample;
    int calibration;
    char *output_file;
    FILE *file_handle;
    AIOSetting setting;
    AIORET_TYPE (*configure)( struct configuration *);
    AIORET_TYPE (*run)( struct configuration *);
} Configuration;

typedef Configuration ADCConfiguration;

Configuration *NewConfiguration( ) 
{
    Configuration *tmp = (Configuration *)calloc(sizeof(Configuration),1);
    if ( !tmp )
        goto out_NewConfiguration;
    out_NewConfiguration:
    return tmp;
}

AIORET_TYPE ConfigSetTimeout( Configuration *config, unsigned timeout )
{
    assert(config);
    if ( !config )
        return -AIOUSB_ERROR_INVALID_CONFIG;
    
    switch ( config->type ) {
    case AIOCONTINUOUSBUF_CONFIG:
        AIOContinuousBufSetTimeout( config->setting->aiobuf, timeout );
        break;
    case ADCCONIGBLOCK_CONFIG:
    default:
        ADCConfigBlockSetTimeout( config->setting->adc , timeout );
    }
}

AIORET_TYPE ConfigSetDebug( Configuration *config, AIOUSB_BOOL debug )
{
    assert(config);
    if ( !config )
        return -AIOUSB_ERROR_INVALID_CONFIG;
    
    switch ( config->type ) {
    case AIOCONTINUOUSBUF_CONFIG:
        AIOContinuousBufSetDebug( config->setting->aiobuf, debug );
        break;
    case ADCCONIGBLOCK_CONFIG:
    default:
        ADCConfigBlockSetDebug( config->setting->adc , debug );
    }
    return AIOUSB_SUCCESS;
}


typedef enum {
    AD_NO_SET_CAL = -1,
    AD_SET_CAL_AUTO = 0,
    AD_SET_CAL_NORMAL,
    AD_SET_CAL_MANUAL
} ADCSetCalFunctions;


typedef struct {
    AIOUSB_BOOL threaded;
    AIOUSB_BOOL debug;
    FILE *outfile;
    int index;
    int timeout;
    int setcal;
} AIOArgument;

AIOArgument *NewAIOArgument()
{
    AIOArgument *tmp = (AIOArgument *)calloc(sizeof(AIOArgument),1);
    if ( !tmp ) 
        return tmp;
    tmp->timeout = 1000;
    return tmp;
}

typedef struct {
    AIOArgument *device_args;
    int number_arguments;
} AIOArguments ;

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOArgumentsAddAIOArgument( AIOArguments *allargs, AIOArgument *arg )
{
    AIORET_TYPE retval  = AIOUSB_SUCCESS;
    allargs->device_args = (AIOArgument *)realloc(allargs->device_args, (allargs->number_arguments++)*(sizeof(AIOArgument)));
    if( !allargs->device_args )
        return -AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
    memcpy(&allargs->device_args[allargs->number_arguments-1], arg, sizeof(AIOArgument));
    return retval;
}

/**
 *
 * needs function for 
 * 
 *  ADC_GetScanV();
 *
 *  ADC_GetScan();
 * 
 */
ADCConfiguration *NewADCConfiguration()
{
    ADCConfiguration *adcconfig = (ADCConfiguration *)calloc(sizeof(ADCConfiguration),1);
    if ( !adcconfig )
        goto out_NewADCConfiguration;
    adcconfig->timeout = 1000;
    adcconfig->discard_first_sample = AIOUSB_FALSE;
    adcconfig->buffer_size = 1000;
 out_NewADCConfiguration:
    return adcconfig;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Performs the configuration in question
 *
 *
 */
AIORET_TYPE ADCConfigurationRun( ADCConfiguration *adcconfig )
{
    /* First setup the ADCConfigBlock */
    ADCConfigBlock *tmp = NULL;
    /* ADCConfigBlock *tmp = NewADCConfigBlockFromJSON(  ); */

    /* Set up the ADCConfigBlock */
    AIOUSBDeviceSetADCConfigBlockFromJSON( );

     /* If Calibration, set that up */
    switch ( adcconfig->calibration ) {
    case CALIBRATION_AUTO:
        ADC_SetCal( adcconfig->device_index, ":AUTO:" );
        break;
    case CALIBRATION_NONE:
        ADC_SetCal( adcconfig->device_index, ":1TO1:" );
        break;
    case CALIBRATION_FILE:
         AIOUSB_ADC_LoadCalTable(adcconfig->device_index, CalFileName);
         break;
    default:
        break;
    } 

    /* Preallocate a buffer */
    adcconfig->buffer  = (char *)malloc( adcconfig->number_scans * determine_size( adcconfig ));

    /* Setup the output */
    if ( adcconfig->output_file ) {
        adcconfig->file_handle = fopen( adcconfig->output_file , "w" );
        if ( !adcconfig->file_handle ) {
            fprintf(stderr,"Unable to open '%s' for writing: %s\n", adcconfig->output_file, sys_errlist[errno] );
            return -2;
        }
    } else {
        adcconfig->file_handle = stdout;
    }

    /* Run data collection */
    if ( adcconfig->FUNCTION == GETSCAN ) {
         for ( i = 0 ; i < adcconfig->number_samples ; i ++ )  {
            result = ADC_GetScan( deviceIndex, counts );
            /* do something with the results */
            if ( adcconfig->easy_output ) { 
                int start = 0;
                for( int i = 0; i < 15 ; i ++ ) {
                    if ( start ++ > 0 ) 
                        fprintf( adcconfig->file_handle, "," );
                    fprintf( adcconfig->file_handle, "%d", counts );
                }
                fprintf( adcconfig->file_handle, "\n", counts );
            } else {
                fwrite(  adcconfig->file_handle);
            }
         }

     } else if ( adcconfig->FUNCTION == GETSCANV  ) {
         for ( i = 0 ; i < adcconfig->number_samples ; i ++ )  {
             result = ADC_GetScan( deviceIndex, counts );
             /* do something with the results */
             fwrite( );
         }
     } else if ( adcconfig->FUNCTION == BULK ) { 
         printf("");
     } else {
         printf("");
     }
}

AIORET_TYPE ADCConfigurationConfigure( ADCConfiguration *adcconfig )
{
    AIOUSB_Reset( adcconfig->device_index  );
    AIOUSB_SetCommTimeout( adcconfig->device_index , adcconfig->timeout );
    AIOUSB_SetDiscardFirstSample( adcconfig->device_index , adcconfig->discard_first_sample );

    /* Setup the ADC */
    ADC_SetConfig( adcconfig->device_index, adcconfig->setting->adc->registers , &adcconfig->setting->adc->size );
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Smart commnad line parsing that sets up the specific devices at the indexes . All that is 
 *        required after parsing the command line is to run the Setup() method.
 *
 */
int aiousb_getoptions( int argc, char **argv)
{
    AIOArguments *retargs = NULL;
    /**
     * command --device index=0,adcconfig=$(cat foo.json),timeout=1000,debug=1
     *         --device index=1,adcconfig=$(cat foo.json),timeout=1000,channel=2=+-2V,sample=getscan,file=foo.csv
     *         --device index=2,aiocontinuousbuf=$(cat cb.json),timeout=2000,setcal=auto,count=3000,sample=getscanv,file=bar.csv
     *         --device index=2,aiocontinuousbuf=$(cat cb.json),timeout=2000,setcal=normal,count=3000,sample=getscanv,file=bar.csv
     *         --device index=3,timeout=1000,setcal=normal,numscans=1000,sample=getscanv,file=output.csv
     *
     * The optional count=3000,action=getscanv will fireup a separate thread and run this and generate 
     */
    /* AIOUSB_Init(); */
    int option_index = 0;
    static struct option long_options[] = {
        {"foo"              , no_argument       , 0,  0 },
        {"listdevices"      , optional_argument , 0,  0 },
        {"device"           , required_argument , 0,  0 },
        {0                  , 0,                 0,  0 }
    };

    typedef enum {
        DEVICE_NUM = 0,
        ADCCONFIG_OPT,
        TIMEOUT_OPT,
        DEBUG_OPT,
        SETCAL_OPT,
        COUNT_OPT,
        SAMPLE_OPT,
        FILE_OPT,
        CHANNEL_OPT
    } DeviceEnum;

    char *const token[] = {
        [DEVICE_NUM]     = "device",
        [ADCCONFIG_OPT]  = "adcconfig",
        [TIMEOUT_OPT]    = "timeout",
        [DEBUG_OPT]      = "debug",
        [SETCAL_OPT]     = "setcal",
        [COUNT_OPT]      = "count",
        [SAMPLE_OPT]     = "sample",
        [FILE_OPT]       = "file",
        [CHANNEL_OPT]    = "channel",
        NULL
    };
#if 0
    char *const token[] = {
        [0]  = "device",
        [1]  = "adcconfig",
        [2]  = "timeout",
        [3]  = "debug",
        [4]  = "setcal",
        [5]  = "count",
        [6]  = "sample",
        [7]  = "file",
        [8]  = "channel",
        0
    };
#endif
    char *subopts;
    char *value;
    char *file;
    int device_number;
    int debug = 1;
    int timeout = 0;
    int error_out = 0;
    /* Can I make this parameter skip through ? */
    Configuration *config;

    while ( 1 ) { 
        int c = getopt_long(argc, argv, "", long_options, &option_index);
        /* printf("%d\n", c ); */
        if (c == -1)
            break;
        switch (c) {
        case 0:

            subopts = optarg;
            int tmp;

            if ( strcmp( long_options[option_index].name,"listdevices" ) == 0 ) {
                /* AIOUSB_ListDevices(); */
                AIOUSB_Init();
                AIODisplayType type;
                if ( optarg ) 
                    if ( strcasecmp(optarg, "terse") == 0 ) {
                        type = TERSE;
                    } else if ( strcasecmp( optarg, "json" ) == 0 ) {
                        type = JSON;
                    } else if ( strcasecmp( optarg, "yaml" ) == 0 ) { 
                        type = YAML;
                    } else {
                        type = BASIC;
                    }
                else
                    type = BASIC;
                AIOUSB_ShowDevices( type );
                exit(1);
            } else if ( strcmp( long_options[option_index].name,"device" ) == 0 ) {
                Configuration *config = NewConfiguration();

                switch ( (tmp = getsubopt(&subopts, token, &value))) {
                case DEVICE_NUM:
                    device_number = atoi(value);
                    break;
                case ADCCONFIG_OPT:
                    {
                        ADCConfigBlock *adc = (ADCConfigBlock *)NewADCConfigBlockFromJSON( value );
                        config->type = ADCCONIGBLOCK_CONFIG;
                        if (!adc ) { 
                            fprintf(stderr,"Error reading JSON for '%s'\n", value );
                            error_out = 1;
                        }
                        ADCConfigBlockCopy( &config->setting , adc );
                    }
                    break;
                case TIMEOUT_OPT:
                    config->timeout = atoi(value);
                    /* config->setting; */
                    ConfigSetTimeout( config, timeout );
                    break;
                case DEBUG_OPT:
                    /* config->debug = ( atoi(value) == 1 ? 1 : 0 ); */
                    ConfigSetDebug( config, debug );
                    break;
                case SETCAL_OPT:
                    /* Calibration mode that can be one of several things */
                    break;
                case COUNT_OPT:
                    /**
                     * Number of samples to take for this system
                     */
                    break;
                case SAMPLE_OPT:
                    /* device which type of function to run */
                    /**
                     * One of these values
                     * getscan
                     * getscanv
                     * getchannel
                     * getchannelv
                     * continuous
                     * bulkacquire
                     */
                    break;
                case FILE_OPT:
                    file = strdup(value);
                    break;
                case CHANNEL_OPT:
                    /* Split up channel where the value is   CHANNEL=RANGE */
                    break;
                case -1:
                    break;
                }
            }
            break;
        default:
            break;
        }
    }
    return 0;
}

int main( int argc, char **argv ) {
    /* GetOptions( opts ); */
    /* device = opts.device; */


    /* char **argv; */
    /* int argc; */
    if ( 0 ) {
    argv = (char **)malloc(10*sizeof(char*));
    argv[0] = (char *)malloc(strlen("/home/jdamon/Projects/aiousb_fixes/AIOUSB_latest_one/AIOUSB/samples/USB-AI16-16/simple_sample") + 1);
    strcpy(argv[0],"/home/jdamon/Projects/aiousb_fixes/AIOUSB_latest_one/AIOUSB/samples/USB-AI16-16/simple_sample");

        argv[1] = (char *)malloc(strlen("--listdevices=terse") + 1);
        /* argv[2] = (char *)malloc(strlen("terse") + 1); */
        argv[2] = NULL;
        strcpy(argv[1],"--listdevices=terse");
        /* strcpy(argv[2],"terse"); */
        argc = 2;
        argv[3] = NULL;
    } else if ( 0 )  {
        argv[1] = (char *)malloc(strlen("--device") + 1);
        argv[2] = (char *)malloc(strlen("device=1,timeout=1000,channel=2=+-2V,sample=getscan,file=foo.csv") + 1);
        strcpy(argv[1],"--device");
        strcpy(argv[2],"device=1,timeout=1000,channel=2=+-2V,sample=getscan,file=foo.csv");
        argc = 3;
        argv[3] = NULL;
    }

    /* Sep API lib */
    char *tmp = get_current_dir_name();
    char *sample_name = (rindex(tmp,'/')+ 1);
    aiousb_getoptions(argc,  argv );
    
    printf( "  %s sample program version %s, %s\n"
            "  This program demonstrates controlling a %s device on\n"
            "  the USB bus. For simplicity, it uses the first such device found\n"
            "  on the bus.\n", sample_name, AIOUSB_GetVersion(), AIOUSB_GetVersionDate(), sample_name );

    exit(1);
    /*
     * MUST call AIOUSB_Init() before any meaningful AIOUSB functions;
     * AIOUSB_GetVersion() above is an exception
     */
    USBDevice *device_list;
    int size = 0;

    AIOUSB_Init();          /* Need this to set up the USB stuff */

#if 0

    /* FindDevices(&device_list, &size, USB_AI16_16A, USB_AI16_16A ); */
    /* FindDevices(&device_list, &size, device, device ); */


    ConfigureDevices( &device_list, &size, getopt_objects );

    

    ADCConfigBlock *nconfig = NewADCConfigBlockFromJSON( json_str );
    if ( !nconfig ) {
        printf("Error reading JSON object\n" );
        exit(1);
    }

    /* ADCConfigBlockCopy( device->cachedConfigBlock, nconfig ); */

    AIOUSBDeviceSetADCConfigBlockFromJSON( device, nconfig );

    AIOUSBDeviceSetADCConfigBlock( device, nconfig );
    DeleteADCConfigBlock( nconfig );
    AIOUSBDevicePutADCConfigBlock( device );

    /* ADCConfigBlock configBlock; */
    /* AIOUSB_InitConfigBlock( &configBlock, deviceIndex, AIOUSB_FALSE ); */
    /* AIOUSB_SetAllGainCodeAndDiffMode( &configBlock, AD_GAIN_CODE_10V, AIOUSB_FALSE ); */
    /* AIOUSB_SetCalMode( &configBlock, AD_CAL_MODE_NORMAL ); */
    /* AIOUSB_SetTriggerMode( &configBlock, 0 ); */
    /* AIOUSB_SetScanRange( &configBlock, 2, 13 ); */
    /* AIOUSB_SetOversample( &configBlock, 0 ); */
    /* result = ADC_SetConfig( deviceIndex, configBlock.registers, &configBlock.size ); */
    /* if( result == AIOUSB_SUCCESS ) { */
    /* 		const int CAL_CHANNEL = 5; */
    /* 		const int MAX_CHANNELS = 128; */
    /* 		const int NUM_CHANNELS = 16; */
    /* 		unsigned short counts[ MAX_CHANNELS ]; */
    /* 		double volts[ MAX_CHANNELS ]; */
    /* 		unsigned char gainCodes[ NUM_CHANNELS ]; */
    /* 		printf( "A/D settings successfully configured\n" ); */

    /* 		/\* */
    /* 		 * demonstrate automatic A/D calibration */
    /* 		 *\/ */

    result = ADC_SetCal( deviceIndex, ":AUTO:" );


    /* if( result == AIOUSB_SUCCESS ) */
    /* 	printf( "Automatic calibration completed successfully\n" ); */
    /* else */
    /* 	printf( "Error '%s' performing automatic A/D calibration\n" */
    /* 	, AIOUSB_GetResultCodeAsString( result ) ); */
    /*
     * verify that A/D ground calibration is correct
     */
    /* ADC_SetOversample( deviceIndex, 0 ); */
    /* ADC_SetScanLimits( deviceIndex, CAL_CHANNEL, CAL_CHANNEL ); */
    /* ADC_ADMode( deviceIndex, 0 /\* TriggerMode *\/, AD_CAL_MODE_GROUND ); */


    result = ADC_GetScan( deviceIndex, counts );

    /* if( result == AIOUSB_SUCCESS ) */
    /* 	printf( "Ground counts = %u (should be approx. 0)\n", counts[ CAL_CHANNEL ] ); */
    /* else */
    /* 	printf( "Error '%s' attempting to read ground counts\n" */
    /* 	, AIOUSB_GetResultCodeAsString( result ) ); */
    /*
     * verify that A/D reference calibration is correct
     */
    /* Now a whole other simulation here */

    /* ADC_ADMode( deviceIndex, 0 /\* TriggerMode *\/, AD_CAL_MODE_REFERENCE ); */

    ADC_ADMode( deviceIndex, 0 , AD_CAL_MODE_REFERENCE );
    result = ADC_GetScan( deviceIndex, counts );

    /* if( result == AIOUSB_SUCCESS ) */
    /*printf( "Reference counts = %u (should be approx. 65130)\n", counts[ CAL_CHANNEL ] ); */
    /* 				else */
    /* 					printf( "Error '%s' attempting to read reference counts\n" */
    /* 					, AIOUSB_GetResultCodeAsString( result ) ); */
    /*
     * demonstrate scanning channels and measuring voltages
     */
    /* for( int channel = 0; channel < NUM_CHANNELS; channel++ ) */
    /*     gainCodes[ channel ] = AD_GAIN_CODE_0_10V; */
    /* ADC_RangeAll( deviceIndex, gainCodes, AIOUSB_TRUE ); */
    /* ADC_SetOversample( deviceIndex, 10 ); */
    /* ADC_SetScanLimits( deviceIndex, 0, NUM_CHANNELS - 1 ); */
    /* ADC_ADMode( deviceIndex, 0 /\* TriggerMode *\/, AD_CAL_MODE_NORMAL ); */

    result = ADC_GetScanV( deviceIndex, volts );
    
    exit(1);


    /* if( result == AIOUSB_SUCCESS ) { */
    /*     printf( "Volts read:\n" ); */
    /*     for( int channel = 0; channel < NUM_CHANNELS; channel++ ) */
    /*         printf( "  Channel %2d = %f\n", channel, volts[ channel ] ); */
    /* } else */
    /*     printf( "Error '%s' performing A/D channel scan\n" */
    /*             , AIOUSB_GetResultCodeAsString( result ) ); */
    /*
     * demonstrate reading a single channel in volts
     */

    /* No point in showing off this part */
    /* instead show how to minimize this channel effect */

    /* result = ADC_GetChannelV( deviceIndex, CAL_CHANNEL, &volts[ CAL_CHANNEL ] ); */


        
    /* Whole other simulation */
    /* Simple bulk simulation */

    /* AIOUSB_Reset( deviceIndex ); */
    /* ADC_SetOversample( deviceIndex, 10 ); */
    /* ADC_SetScanLimits( deviceIndex, 0, NUM_CHANNELS - 1 ); */
    /* AIOUSB_SetStreamingBlockSize( deviceIndex, 100000 ); */
    /* const int BULK_BYTES = 100000 /\* scans *\/ */
    /* 	* NUM_CHANNELS */
    /* 	* sizeof( unsigned short ) /\* bytes / sample *\/ */
    /* 	* 11 /\* 1 sample + 10 oversamples *\/; */
    /* const double CLOCK_SPEED = 100000;	// Hz */
    /* printf("Allocating %d Bytes\n", BULK_BYTES ); */
    /* unsigned short *const dataBuf = ( unsigned short * ) malloc( BULK_BYTES ); */
    /* if( dataBuf != 0 ) { */
    /*
     * make sure counter is stopped
     */
    /* double clockHz = 0; */

    CTR_StartOutputFreq( deviceIndex, 0, &clockHz );

    /*
     * configure A/D for timer-triggered acquisition
     */
    ADC_ADMode( deviceIndex, AD_TRIGGER_SCAN | AD_TRIGGER_TIMER, AD_CAL_MODE_NORMAL );
        
    /*
     * start bulk acquire; ADC_BulkAcquire() will take care of starting
     * and stopping the counter; but we do have to tell it what clock
     * speed to use, which is why we call AIOUSB_SetMiscClock()
     */
    AIOUSB_SetMiscClock( deviceIndex, CLOCK_SPEED );
    result = ADC_BulkAcquire( deviceIndex, BULK_BYTES, dataBuf );

    /* if( result == AIOUSB_SUCCESS ) */
    /*     printf( "Started bulk acquire of %d bytes\n", BULK_BYTES ); */
    /* 					else */
    /* 						printf( "Error '%s' attempting to start bulk acquire of %d bytes\n" */
    /* 						, AIOUSB_GetResultCodeAsString( result ) */
    /* 							, BULK_BYTES ); */

    /*
     * use bulk poll to monitor progress
     */
    if( result == AIOUSB_SUCCESS ) {
        unsigned long bytesRemaining = BULK_BYTES;
        for( int seconds = 0; seconds < 100; seconds++ ) {
            sleep( 1 );
            result = ADC_BulkPoll( deviceIndex, &bytesRemaining );
            if( result == AIOUSB_SUCCESS ) {
                printf( "  %lu bytes remaining\n", bytesRemaining );
                if( bytesRemaining == 0 )
                    break;	// from for()
            } else {
                printf( "Error '%s' polling bulk acquire progress\n", 
                        AIOUSB_GetResultCodeAsString( result ) );
                break;
            }
        }
            
        /*
         * turn off timer-triggered mode
         */
        ADC_ADMode( deviceIndex, 0, AD_CAL_MODE_NORMAL );
            
        /*
         * if all the data was apparently received, scan it for zeros; it's
         * unlikely that any of the data would be zero, so any zeros, particularly
         * a large block of zeros would suggest that the data is not valid
         */
        if(
           result == AIOUSB_SUCCESS
           && bytesRemaining == 0
           ) {
            AIOUSB_BOOL anyZeroData = AIOUSB_FALSE;
            int zeroIndex = -1;
            for( int index = 0; index < BULK_BYTES / ( int ) sizeof( unsigned short ); index++ ) {
                if( dataBuf[ index ] == 0 ) {
                    anyZeroData = AIOUSB_TRUE;
                    if( zeroIndex < 0 )
                        zeroIndex = index;
                } else {
                    if( zeroIndex >= 0 ) {
                        printf( "  Zero data from index %d to %d\n", zeroIndex, index - 1 );
                        zeroIndex = -1;
                    }
                }
            }
            if( anyZeroData == AIOUSB_FALSE )
                printf( "Successfully bulk acquired %d bytes\n", BULK_BYTES );
        } else
            printf( "Failed to bulk acquire %d bytes\n", BULK_BYTES );
    }
        
    free( dataBuf );
    /* }	// if( dataBuf ... */
    /* } else */
    /*       printf( "Error '%s' setting A/D configuration\n" */
    /* 					, AIOUSB_GetResultCodeAsString( result ) ); */
    /* 			} else */
    /* 				printf( "Failed to find USB-AI16-16A device\n" ); */
    /* 		} else */
    /* 			printf( "No ACCES devices found on USB bus\n" ); */
        
    /*
     * MUST call AIOUSB_Exit() before program exits,
     * but only if AIOUSB_Init() succeeded
     */
#endif

    AIOUSB_Exit();

}	// main()


/* end of file */
