#include "ADCConfigBlock.h"
#include "AIOUSBDevice.h"
#include "AIOUSB_Core.h"
#include "cJSON.h"

#ifdef __cplusplus
namespace AIOUSB {
#endif






/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockCopy( ADCConfigBlock *to, ADCConfigBlock *from ) 
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    if ( from->size <  AD_CONFIG_REGISTERS )
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    
    to->device   = from->device;
    to->size     = from->size ;
    to->testing  = from->testing;

    memcpy( to->registers, from->registers, AD_MAX_CONFIG_REGISTERS +1 );

    return result;
}

/*----------------------------------------------------------------------------*/
AIOUSBDevice *ADCConfigBlockGetAIOUSBDevice( ADCConfigBlock *obj , AIORET_TYPE *result ) 
{
    if (!obj ) {
        *result = -AIOUSB_ERROR_DEVICE_NOT_FOUND;
        return NULL;
    }

    return obj->device;
}

/*----------------------------------------------------------------------------*/
AIORESULT _check_ADCConfigBlock( ADCConfigBlock *obj )
{
    AIORESULT result = AIOUSB_SUCCESS;
    if ( !obj ) {
        result = AIOUSB_ERROR_INVALID_DATA;
    }
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetDevice( ADCConfigBlock *obj, AIOUSBDevice *dev )
{
    AIORESULT result = _check_ADCConfigBlock( obj );
    if ( result != AIOUSB_SUCCESS )
        return result;
    obj->device = dev;
    return result;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief initializes an ADCConfigBlock using parameters from the AIOUSBDevice
 */
AIORET_TYPE ADCConfigBlockInitialize( ADCConfigBlock *config , AIOUSBDevice *dev) 
{
    assert(config);
    assert(dev);
    if ( !config )
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;
    if ( !dev ) 
        return -AIOUSB_ERROR_INVALID_DEVICE;

    config->device        = dev;
    config->size          = dev->ConfigBytes;
    config->testing       = dev->testing;
    config->timeout       = dev->commTimeout;

    config->mux_settings.ADCMUXChannels       = dev->ADCMUXChannels;
    config->mux_settings.ADCChannelsPerGroup  = dev->ADCChannelsPerGroup;

    memset(config->registers,0, AD_CONFIG_REGISTERS );
    return AIOUSB_SUCCESS;
}

int _adcblock_valid_channel_settings(AIORET_TYPE in, ADCConfigBlock *config , int ADCMUXChannels )
{
    if ( in != AIOUSB_SUCCESS ) 
        return in;
    
    int result = 1;
    int startChannel,endChannel;
    
    for(int channel = 0; channel < AD_NUM_GAIN_CODE_REGISTERS; channel++) {
        if(( config->registers[ AD_CONFIG_GAIN_CODE + channel ] & ~( unsigned char )(AD_DIFFERENTIAL_MODE | AD_GAIN_CODE_MASK)) != 0 ) {
            return 0;
        }
    }
    
    endChannel = ADCConfigBlockGetEndChannel( config );
    if ( endChannel < 0 ) 
        return -AIOUSB_ERROR_INVALID_ADCCONFIG_CHANNEL_SETTING;
    
    startChannel = ADCConfigBlockGetStartChannel( config );
    if ( endChannel < 0 ) 
        return -AIOUSB_ERROR_INVALID_ADCCONFIG_CHANNEL_SETTING;
    
    if( endChannel >= (int)ADCMUXChannels || startChannel > endChannel ) {
        result = -AIOUSB_ERROR_INVALID_ADCCONFIG_CHANNEL_SETTING;
    }
    
    return result;
}


/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetTesting( ADCConfigBlock *obj, AIOUSB_BOOL testing ) 
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    assert(obj);
    if ( !obj ) 
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    obj->testing = testing;
    return result;
}

/*----------------------------------------------------------------------------*/
unsigned char *ADCConfigBlockGetRegisters( ADCConfigBlock *config )
{
    assert(config);
    return config->registers;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetRangeSingle( ADCConfigBlock *config, unsigned long channel, unsigned char gainCode )
{
    assert(config && config->device && config->size );
    AIORET_TYPE result = AIOUSB_SUCCESS;

    if ( !config || !config->device || !config->size )
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;

    if ( !VALID_ENUM( ADGainCode, gainCode ) ) 
        return -AIOUSB_ERROR_INVALID_PARAMETER;

    if (!AIOUSB_Lock() ) 
        return -AIOUSB_ERROR_INVALID_MUTEX;
    
    AIOUSBDevice *deviceDesc = ADCConfigBlockGetAIOUSBDevice( config , &result );
    if (result != AIOUSB_SUCCESS )
        return -AIOUSB_ERROR_INVALID_DEVICE;

    if( channel > AD_MAX_CHANNELS ||  channel >= deviceDesc->ADCMUXChannels )
        return -AIOUSB_ERROR_INVALID_PARAMETER;

    int reg = AD_CONFIG_GAIN_CODE + channel / deviceDesc->ADCChannelsPerGroup;

    config->registers[ reg ] = gainCode;

    AIOUSB_UnLock();
    return result;
}


/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetRegister( ADCConfigBlock *config, unsigned reg, unsigned char value )
{
    assert(config);
    if ( reg > AD_MAX_CONFIG_REGISTERS )
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    config->registers[reg] = value;
    return AIOUSB_SUCCESS;
}


/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockGetTesting( const ADCConfigBlock *obj) 
{
    assert(obj);
    if ( !obj ) 
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    return obj->testing;
}

/** 
 * @brief 
 * @param config 
 * @param deviceDesc 
 * @param size 
 */
/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockInit(ADCConfigBlock *config, AIOUSBDevice *deviceDesc, unsigned size )
{
    if ( !config )
        return AIOUSB_ERROR_INVALID_PARAMETER;

    config->device = deviceDesc;
    config->size = size;
    config->testing = AIOUSB_FALSE;
    config->timeout = deviceDesc->commTimeout;
    memset(config->registers,(unsigned char)AD_GAIN_CODE_0_5V,16 );
    
    config->registers[AD_CONFIG_CAL_MODE] = AD_CAL_MODE_NORMAL;
    config->registers[AD_CONFIG_TRIG_COUNT] = AD_TRIGGER_CTR0_EXT | AD_TRIGGER_SCAN | AD_TRIGGER_TIMER;
    config->registers[AD_CONFIG_START_END] = 0xF0;
    config->registers[AD_CONFIG_MUX_START_END] = 0;
    config->registers[AD_CONFIG_START_STOP_CHANNEL_EX] = 0;
    
    return AIOUSB_SUCCESS;
}


/*----------------------------------------------------------------------------*/
void ADC_VerifyAndCorrectConfigBlock( ADCConfigBlock *configBlock , AIOUSBDevice *deviceDesc  )
{
    unsigned channel;
    AIOUSB_Lock();
    for(channel = 0; channel < AD_NUM_GAIN_CODE_REGISTERS; channel++) {
        if(( configBlock->registers[ AD_CONFIG_GAIN_CODE + channel ] & 
             ~( unsigned char )(AD_DIFFERENTIAL_MODE | AD_GAIN_CODE_MASK )
             ) != 0 ) { 
            configBlock->registers[ AD_CONFIG_GAIN_CODE + channel ] = FIRST_ENUM(ADGainCode);
        }
    }
                    
    const unsigned char calMode = configBlock->registers[ AD_CONFIG_CAL_MODE ];
    if( calMode != AD_CAL_MODE_NORMAL && calMode != AD_CAL_MODE_GROUND && calMode != AD_CAL_MODE_REFERENCE )
        configBlock->registers[ AD_CONFIG_CAL_MODE ] = AD_CAL_MODE_NORMAL;
                    
    if((configBlock->registers[ AD_CONFIG_TRIG_COUNT ] & ~AD_TRIGGER_VALID_MASK) != 0)
        configBlock->registers[ AD_CONFIG_TRIG_COUNT ] = 0;
                    
    const unsigned endChannel = ADCConfigBlockGetEndChannel( configBlock );
    if( endChannel >= ( unsigned )deviceDesc->ADCMUXChannels  ||
        ADCConfigBlockGetStartChannel( configBlock ) > (int)endChannel )
        ADCConfigBlockSetScanRange( configBlock, 0, deviceDesc->ADCMUXChannels - 1);
                    
    /* deviceDesc->cachedConfigBlock = configBlock; */
    AIOUSB_UnLock();

}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetAllGainCodeAndDiffMode(ADCConfigBlock *config, unsigned gainCode, AIOUSB_BOOL differentialMode)
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    if (!config || !config->device || !config->size ) 
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;

    if ( !VALID_ENUM( ADGainCode, gainCode ) ) 
        return -AIOUSB_ERROR_INVALID_PARAMETER;

    if(differentialMode)
        gainCode |= AD_DIFFERENTIAL_MODE;
    unsigned channel;
    for(channel = 0; channel < AD_NUM_GAIN_CODE_REGISTERS; channel++)
        config->registers[ AD_CONFIG_GAIN_CODE + channel ] = gainCode;

    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE  ADCConfigBlockGetGainCode(const ADCConfigBlock *config, unsigned channel)
{
    if ( !config )
        return -AIOUSB_ERROR_INVALID_DATA;

    unsigned gainCode = FIRST_ENUM(ADGainCode);
    if( config != 0 && config->device != 0 &&   config->size != 0 ) { 
        AIOUSBDevice *deviceDesc = ( AIOUSBDevice* )config->device;
        if(channel < AD_MAX_CHANNELS && channel < deviceDesc->ADCMUXChannels) {
            assert(deviceDesc->ADCChannelsPerGroup != 0);
            gainCode = (config->registers[ AD_CONFIG_GAIN_CODE + channel / deviceDesc->ADCChannelsPerGroup ]
                        & ( unsigned char )AD_GAIN_CODE_MASK
                        );
        }
    }
    return (AIORET_TYPE)gainCode;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetGainCode(ADCConfigBlock *config, unsigned channel, unsigned char gainCode)
{
    if (!config || !config->device || !config->size ) 
        return -AIOUSB_ERROR_INVALID_DATA;
    if (!VALID_ENUM(ADGainCode,gainCode ) )
        return -AIOUSB_ERROR_INVALID_PARAMETER;

    AIORET_TYPE result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = ADCConfigBlockGetAIOUSBDevice( config , &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }
    if ( !deviceDesc->ADCChannelsPerGroup  )
        return  -AIOUSB_ERROR_INVALID_DEVICE_SETTING;

    if (channel < AD_MAX_CHANNELS && channel < deviceDesc->ADCMUXChannels) {
        int reg = AD_CONFIG_GAIN_CODE + channel / deviceDesc->ADCChannelsPerGroup;
        if ( reg > AD_NUM_GAIN_CODES )
            return -AIOUSB_ERROR_INVALID_ADCCONFIG_REGISTER_SETTING;

        config->registers[ reg ] = (config->registers[ reg ] & 
                                    ~( unsigned char )AD_GAIN_CODE_MASK) | 
                                   ( unsigned char )(gainCode & AD_GAIN_CODE_MASK);
    }
    
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetScanRange(ADCConfigBlock *config, unsigned startChannel, unsigned endChannel)
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    if (!config || !config->device || !config->size )
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;

    AIOUSBDevice * deviceDesc = ( AIOUSBDevice* )config->device;
    if( endChannel < AD_MAX_CHANNELS && 
        endChannel < deviceDesc->ADCMUXChannels &&
        startChannel <= endChannel
        ) {
        if(config->size == AD_MUX_CONFIG_REGISTERS) {
            /*
             * this board has a MUX, so support more channels
             */
            config->registers[ AD_CONFIG_START_END ] = ( unsigned char )((endChannel << 4) | (startChannel & 0x0f));
            config->registers[ AD_CONFIG_MUX_START_END ] = ( unsigned char )((endChannel & 0xf0) | ((startChannel >> 4) & 0x0f));
        } else {
            /*
             * this board doesn't have a MUX, so support base
             * number of channels
             */
            config->registers[ AD_CONFIG_START_END ] = ( unsigned char )((endChannel << 4) | startChannel);
        }
    }

 return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetCalMode(ADCConfigBlock *config, ADCalMode calMode)
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    if ( !config || config->size == 0 )
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;
     
    if( !VALID_ENUM(ADCalMode, calMode )  )
        return -AIOUSB_ERROR_INVALID_PARAMETER;

    config->registers[ AD_CONFIG_CAL_MODE ] = ( unsigned char )calMode;
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockGetCalMode(const ADCConfigBlock *config)
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;

    if (!config || !config->device || !config->size ) 
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;

    retval = config->registers[ AD_CONFIG_CAL_MODE ];
    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockGetStartChannel( const ADCConfigBlock *config)
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    if ( !config || !config->device || config->size == 0 )
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;


    if(config->size == AD_MUX_CONFIG_REGISTERS)
        result = (AIORET_TYPE)((config->registers[ AD_CONFIG_MUX_START_END ] & 0x0f) << 4) | (config->registers[ AD_CONFIG_START_END ] & 0xf);
    else
        result  = (AIORET_TYPE)(config->registers[ AD_CONFIG_START_END ] & 0xf);
 
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockGetEndChannel( const ADCConfigBlock *config)
{
    AIORET_TYPE endChannel = AIOUSB_SUCCESS;
    if (!config || !config->device || !config->size ) 
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;

    if ( config->size == AD_MUX_CONFIG_REGISTERS)
        endChannel = (AIORET_TYPE)(config->registers[ AD_CONFIG_MUX_START_END ] & 0xf0)
            | (config->registers[ AD_CONFIG_START_END ] >> 4);
    else
        endChannel = (AIORET_TYPE)(config->registers[ AD_CONFIG_START_END ] >> 4);

    return endChannel;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockGetOversample( const ADCConfigBlock *config )
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    if ( !config )
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;
    result = config->registers[ AD_CONFIG_OVERSAMPLE ];
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetOversample( ADCConfigBlock *config, unsigned overSample )
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    if ( !config )
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;
    config->registers[ AD_CONFIG_OVERSAMPLE ] = ( unsigned char )overSample;
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockGetTriggerMode(const ADCConfigBlock *config)
{
    if (!config || !config->device || !config->size ) 
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;

    AIORET_TYPE triggerMode = 0;
    triggerMode = config->registers[ AD_CONFIG_TRIG_COUNT ] & ( unsigned char )AD_TRIGGER_VALID_MASK;
    return triggerMode;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetTriggerMode(ADCConfigBlock *config, unsigned triggerMode )
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    if (!config || !config->device || !config->size ) 
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;
    if ( (triggerMode  & ~AD_TRIGGER_VALID_MASK ) != 0 )
        return -AIOUSB_ERROR_INVALID_DATA;

    config->registers[ AD_CONFIG_TRIG_COUNT ] = triggerMode;
    return result;
}



/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetDifferentialMode(ADCConfigBlock *config, unsigned channel, AIOUSB_BOOL differentialMode)
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    if ( !config || config->device != 0 || !config->size )
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;
    
    AIOUSBDevice * deviceDesc = ( AIOUSBDevice* )config->device;
    if ( channel >= deviceDesc->ADCMUXChannels || channel >= AD_MAX_CHANNELS )
        return -AIOUSB_ERROR_INVALID_DATA;

    if ( !deviceDesc->ADCChannelsPerGroup ) 
        return -AIOUSB_ERROR_INVALID_DEVICE_FUNCTIONAL_PARAMETER;

    
    int reg = AD_CONFIG_GAIN_CODE + channel / deviceDesc->ADCChannelsPerGroup;
    if ( reg < AD_NUM_GAIN_CODE_REGISTERS ) 
        return -AIOUSB_ERROR_INVALID_ADCCONFIG_REGISTER_SETTING;
    if(differentialMode)
        config->registers[ reg ] |= ( unsigned char )AD_DIFFERENTIAL_MODE;
    else
        config->registers[ reg ] &= ~( unsigned char )AD_DIFFERENTIAL_MODE;

    AIOUSB_UnLock();
    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE _assist_trigger_select( ADCConfigBlock *config, AIOUSB_BOOL val , int setting )
{
    assert(config);
    if ( !config )
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;
    config->registers[AD_REGISTER_TRIG_COUNT] = (config->registers[AD_REGISTER_TRIG_COUNT] & ~setting) | ( val ? setting : 0 );
    return AIOUSB_SUCCESS;
}

/*------------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetTriggerEdge( ADCConfigBlock *config, AIOUSB_BOOL val )
{
    return _assist_trigger_select(config, val, AD_TRIGGER_FALLING_EDGE);
}

/*------------------------------------------------------------------------------*/
const char *get_gain_code( int code )
{
    switch( code ) {
    case FIRST_ENUM(ADGainCode):
        return "0-10V";
    case AD_GAIN_CODE_10V:
        return "+-10V";
    case AD_GAIN_CODE_0_5V:
        return "0-5V";
    case AD_GAIN_CODE_5V:
        return "+-5V";
    case AD_GAIN_CODE_0_2V:
        return "0-2V";
    case AD_GAIN_CODE_2V:
        return "+-2V";
    case AD_GAIN_CODE_0_1V:
        return "0-1V";
    case AD_GAIN_CODE_1V:
        return "+-1V";
    default:
        return "Unknown";
    }
}

const char *get_cal_mode( int code )
{
    switch ( code ) {
    case AD_CAL_MODE_NORMAL:
        return "Normal";
    case AD_CAL_MODE_GROUND:
        return "Ground";
    case AD_CAL_MODE_REFERENCE:
        return "Reference";
    case AD_CAL_MODE_BIP_GROUND:
        return "BIP Reference";
    default:
        return "Unknown";
    }
}

/*----------------------------------------------------------------------------*/
const char *get_trigger_mode( int code )
{
   if (code & AD_TRIGGER_CTR0_EXT) {
       return "external";
   } else if (code & AD_TRIGGER_TIMER) {
       return "counter";
   } else {
       return "sw";
   }
}

/*----------------------------------------------------------------------------*/
const char *get_edge_mode(int code)
{
    if ( code & AD_TRIGGER_FALLING_EDGE) {
        return "falling-edge";
    } else {
        return "rising-edge";
    }
}

/*----------------------------------------------------------------------------*/
const char *get_scan_mode(int code)
{
  if ( code & AD_TRIGGER_SCAN) {
      return "all-channels";
   } else {
      return "single-channel";
  }
  
}

#define CALIBRATION_STRING "calibration"
#define TRIGGER_STRING "trigger"
#define REFERENCE_STRING "reference"
#define EDGE_STRING "edge"
#define REFCHANNEL_STRING "refchannel"
#define STARTCHANNEL_STRING "start_channel"
#define ENDCHANNEL_STRING "end_channel"
#define GAIN_STRING "gain"
#define CHANNELS_STRING "channels"
#define CONFIG_STRING "adcconfig"
#define OVERSAMPLE_STRING "oversample"

/*----------------------------------------------------------------------------*/
/**
 * @verbatim
 * ---
 * adcconfig:
 *   channels:
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   calibration: Normal
 *   trigger:
 *     edge: falling edge
 *     refchannel: all-channels
 *     reference: external
 *   oversample: 201
 * @endverbatim
 */
char *ADCConfigBlockToYAML(ADCConfigBlock *config)
{
    int i;
    char tmpbuf[2048] = {0};

    sprintf( &tmpbuf[strlen(tmpbuf)], "---\n%s:\n", CONFIG_STRING );
    sprintf( &tmpbuf[strlen(tmpbuf)], "  %s:\n", CHANNELS_STRING );
    for(i = 0; i <= 15; i++)
        sprintf( &tmpbuf[strlen(tmpbuf)], "    - %s: %s\n", GAIN_STRING, get_gain_code(config->registers[i]));

    sprintf( &tmpbuf[strlen(tmpbuf)],"  %s: %s\n", CALIBRATION_STRING, get_cal_mode( config->registers[AD_REGISTER_CAL_MODE] ));
    sprintf( &tmpbuf[strlen(tmpbuf)],
             "  %s:\n     %s: %s\n", 
             TRIGGER_STRING, 
             REFERENCE_STRING, 
             get_trigger_mode( config->registers[AD_REGISTER_TRIG_COUNT] ));

    sprintf( &tmpbuf[strlen(tmpbuf)], "     %s: %s\n", EDGE_STRING, get_edge_mode( config->registers[AD_REGISTER_TRIG_COUNT] ) );
    sprintf( &tmpbuf[strlen(tmpbuf)], "     %s: %s\n", REFCHANNEL_STRING, get_scan_mode(config->registers[AD_REGISTER_TRIG_COUNT] ));
    sprintf( &tmpbuf[strlen(tmpbuf)], "  %s: %d\n", STARTCHANNEL_STRING, ADCConfigBlockGetStartChannel( config ));
    sprintf( &tmpbuf[strlen(tmpbuf)], "  %s: %d\n", ENDCHANNEL_STRING, ADCConfigBlockGetEndChannel( config ));
    sprintf( &tmpbuf[strlen(tmpbuf)], "  %s: %d\n", OVERSAMPLE_STRING, ADCConfigBlockGetOversample( config ));

    return strdup(tmpbuf);
}


/*---------------------  Default settings for JSON read  ---------------------*/
EnumStringLookup Gains[] = {
    { AD_GAIN_CODE_0_10V , "0-10V" , STRINGIFY(AD_GAIN_CODE_0_10V)  }, /* Default value */
    { AD_GAIN_CODE_10V   , "+-10V" , STRINGIFY(AD_GAIN_CODE_10V  )  },
    { AD_GAIN_CODE_0_5V  , "0-5V"  , STRINGIFY(AD_GAIN_CODE_0_5V )  },
    { AD_GAIN_CODE_5V    , "+-5V"  , STRINGIFY(AD_GAIN_CODE_5V   )  },
    { AD_GAIN_CODE_0_2V  , "0-2V"  , STRINGIFY(AD_GAIN_CODE_0_2V )  },
    { AD_GAIN_CODE_2V    , "+-2V"  , STRINGIFY(AD_GAIN_CODE_2V   )  },
    { AD_GAIN_CODE_0_1V  , "0-1V"  , STRINGIFY(AD_GAIN_CODE_0_1V )  },
    { AD_GAIN_CODE_1V    , "+-1V"  , STRINGIFY(AD_GAIN_CODE_1V   )  },
};

EnumStringLookup Calibrations[] = {
    {AD_CAL_MODE_NORMAL     , "Normal"        ,  STRINGIFY(AD_CAL_MODE_NORMAL    )  }, /* Default */
    {AD_CAL_MODE_GROUND     , "Ground"        ,  STRINGIFY(AD_CAL_MODE_GROUND    )  },
    {AD_CAL_MODE_REFERENCE  , "Reference"     ,  STRINGIFY(AD_CAL_MODE_REFERENCE )  },
    {AD_CAL_MODE_BIP_GROUND , "BIP Reference" ,  STRINGIFY(AD_CAL_MODE_BIP_GROUND)  }
};

EnumStringLookup ReferenceModes[] = {
    {AD_TRIGGER_TIMER   , "counter"  , STRINGIFY(AD_TRIGGER_TIMER    ) }, /* Default */
    {AD_TRIGGER_CTR0_EXT, "external" , STRINGIFY(AD_TRIGGER_CTR0_EXT ) },
    {0                  , "sw"       , STRINGIFY(0                   ) },
};

EnumStringLookup Edges[] = {
    { 1, "falling-edge", "1" }, /* Default */
    { 0, "rising-edge" , "0" }
};

EnumStringLookup RefChannels[] = {
    { 1, "all-channels"   , "1" }, /* Default */
    { 0, "single-channel" , "0" }
};

EnumStringLookup StartChannel[] = {
    { 0, "0",  "0" }
};

EnumStringLookup EndChannel[] = {
    { 15, "15",  "15" }
};

EnumStringLookup Oversample[] = {
    { 0, "0", "0" }
};
/*-----------------------  End settings for JSON read  -----------------------*/

/*----------------------------------------------------------------------------*/
char *ADCConfigBlockToJSON(ADCConfigBlock *config)
{
    int i;
    char tmpbuf[2048] = {0};

    sprintf( &tmpbuf[strlen(tmpbuf)], "{\"%s\":{", CONFIG_STRING );
    sprintf( &tmpbuf[strlen(tmpbuf)], "\"%s\":[", CHANNELS_STRING );
    for(i = 0; i <= 14; i++)
        sprintf( &tmpbuf[strlen(tmpbuf)], "{\"%s\":\"%s\"},", GAIN_STRING, get_gain_code(config->registers[i]));
    sprintf( &tmpbuf[strlen(tmpbuf)], "{\"%s\":\"%s\"}],", GAIN_STRING, get_gain_code(config->registers[15]));

    sprintf( &tmpbuf[strlen(tmpbuf)],"\"%s\":\"%s\",", CALIBRATION_STRING, get_cal_mode( config->registers[AD_REGISTER_CAL_MODE] ));
    sprintf( &tmpbuf[strlen(tmpbuf)],
             "\"%s\":{\"%s\":\"%s\",", 
             TRIGGER_STRING, 
             REFERENCE_STRING, 
             get_trigger_mode( config->registers[AD_REGISTER_TRIG_COUNT] ));

    sprintf( &tmpbuf[strlen(tmpbuf)], "\"%s\":\"%s\",", EDGE_STRING, get_edge_mode( config->registers[AD_REGISTER_TRIG_COUNT] ) );
    sprintf( &tmpbuf[strlen(tmpbuf)], "\"%s\":\"%s\"", REFCHANNEL_STRING, get_scan_mode(config->registers[AD_REGISTER_TRIG_COUNT] ));
    sprintf( &tmpbuf[strlen(tmpbuf)], "},");
    sprintf( &tmpbuf[strlen(tmpbuf)], "\"%s\":\"%d\",", STARTCHANNEL_STRING, ADCConfigBlockGetStartChannel( config ));
    sprintf( &tmpbuf[strlen(tmpbuf)], "\"%s\":\"%d\",", ENDCHANNEL_STRING, ADCConfigBlockGetEndChannel( config ));
    sprintf( &tmpbuf[strlen(tmpbuf)], "\"%s\":\"%d\"", OVERSAMPLE_STRING, ADCConfigBlockGetOversample( config ));

    strcat(tmpbuf,"}}");

    return strdup(tmpbuf);
}

/* AIORET_TYPE _assist_trigger_select( ADCConfigBlock *config, AIOUSB_BOOL val , int setting ) */
/* { */
/*     assert(config); */
/*     if ( !config ) */
/*         return -AIOUSB_ERROR_INVALID_ADCCONFIG; */
/*     config->registers[AD_REGISTER_TRIG_COUNT] = (config->registers[AD_REGISTER_TRIG_COUNT] & ~setting) | ( val ? setting : 0 ); */
/* } */


AIORET_TYPE ADCConfigBlockSetScanAllChannels(  ADCConfigBlock *config, AIOUSB_BOOL val )
{
    return _assist_trigger_select(config, val, AD_TRIGGER_SCAN);
}

AIORET_TYPE ADCConfigBlockSetTriggerReference( ADCConfigBlock *config, int val )
{
    return _assist_trigger_select(config, val, val );
}

/*----------------------------------------------------------------------------*/
cJSON *ADCConfigBlockGetJSONValueOrDefault( cJSON *config,
                                            char const *key, 
                                            EnumStringLookup *lookup, 
                                            size_t size
                                            )
{
    static cJSON retval;
    cJSON *tmp;
    int found = 0;

    if ( config && (tmp = cJSON_GetObjectItem(config, key ) ) ) {
        /* \"calibration\":\"Normal\" */
        char *foo = tmp->valuestring;
        for ( size_t j = 0; j < size ; j ++ ) {
            if ( strcasecmp(foo, lookup[j].str ) == 0 )  {
                retval.valuestring = (char *)lookup[j].strvalue;
                retval.valueint = lookup[j].value;
                retval.valuedouble = (double)lookup[j].value;
                found = 1;
                break;
            }
        }
    }
    if (!found ) {
        retval.valuestring = (char *)lookup[0].strvalue;
        retval.valueint = lookup[0].value;
        retval.valuedouble = (double)lookup[0].value;
    }
    return &retval;
}

/*------------------------------------------------------------------------------*/
ADCConfigBlock *ADCConfigBlockFromJSON( char *str )
{
    ADCConfigBlock *adc = (ADCConfigBlock *)calloc(sizeof(ADCConfigBlock),1);
    adc->size = 20;
    cJSON *json;
    json = cJSON_Parse(str);

    if (!json ) 
        return NULL;
    cJSON *adcconfig = cJSON_GetObjectItem(json,"adcconfig");

    if (!adcconfig )
        return NULL;
    cJSON *tmp;
    /* FIRST handle the mux settings if need be */
    /* mux_settings\":{\"adc_channels_per_group\":\"4\",\"adc_mux_channels\":\"0\"},\"start_channel\":\"0\",\"oversample\":\"201\"}}"; */

    if ( (tmp = cJSON_GetObjectItem(adcconfig,"mux_settings") ) ) {
        int found = 0;
        cJSON *muxsettings;
        if ( (muxsettings = cJSON_GetObjectItem(tmp,"adc_channels_per_group") ) ) {
            found ++;
            adc->mux_settings.ADCChannelsPerGroup = atoi( muxsettings->valuestring );
        }

        if ( (muxsettings = cJSON_GetObjectItem(tmp,"adc_mux_channels") ) ) {
            found ++;
            adc->mux_settings.ADCMUXChannels = atoi( muxsettings->valuestring );
        }

        if ( !found ) 
            adc->mux_settings.ADCChannelsPerGroup = adc->mux_settings.ADCMUXChannels = -1;
    }

    if ( (tmp = cJSON_GetObjectItem(adcconfig,"channels") ) ) {
        int i;
        for ( i=0 ;i < cJSON_GetArraySize(tmp); i++ ) {
            cJSON *subitem = cJSON_GetArrayItem(tmp,i);
            cJSON *obj = cJSON_GetObjectItem( subitem,"gain");
            if ( obj ) {
                char *foo = obj->valuestring ;
                int val = -1;
                for ( size_t j = 0; j < sizeof(Gains)/sizeof(EnumStringLookup) ; j ++ ) {
                    if ( strcasecmp(foo, Gains[j].str ) == 0 )  {
                        val = Gains[j].value;
                        break;
                    }
                }
                if ( val >= 0 )
                    ADCConfigBlockSetGainCode( adc, i, val );
            }
        }
    }

    tmp = ADCConfigBlockGetJSONValueOrDefault( adcconfig, 
                                               "calibration", 
                                               Calibrations, 
                                               sizeof(Calibrations)/sizeof(EnumStringLookup)
                                               );
    ADCConfigBlockSetCalMode( adc, (ADCalMode)tmp->valueint ); 

    cJSON *trigger;

    trigger = cJSON_GetObjectItem(adcconfig,"trigger" );

    tmp = ADCConfigBlockGetJSONValueOrDefault( trigger, 
                                               "edge", 
                                               Edges, 
                                               sizeof(Edges)/sizeof(EnumStringLookup)
                                               );
    ADCConfigBlockSetTriggerEdge( adc, tmp->valueint );

    tmp = ADCConfigBlockGetJSONValueOrDefault( trigger, 
                                               "refchannel", 
                                               RefChannels, 
                                               sizeof(RefChannels)/sizeof(EnumStringLookup)
                                               );
    ADCConfigBlockSetScanAllChannels( adc, tmp->valueint );

    tmp =  ADCConfigBlockGetJSONValueOrDefault( trigger, 
                                               "reference", 
                                                ReferenceModes, 
                                                sizeof(ReferenceModes)/sizeof(EnumStringLookup)
                                                );

    tmp =  ADCConfigBlockGetJSONValueOrDefault( adcconfig, 
                                               "start_channel", 
                                                StartChannel, 
                                                sizeof(StartChannel)/sizeof(EnumStringLookup)
                                                );

    ADCConfigBlockSetScanRange( adc, tmp->valueint , ADCConfigBlockGetEndChannel(adc) );
    tmp =  ADCConfigBlockGetJSONValueOrDefault( adcconfig, 
                                               "end_channel", 
                                                EndChannel, 
                                                sizeof(EndChannel)/sizeof(EnumStringLookup)
                                                );
    ADCConfigBlockSetScanRange( adc, ADCConfigBlockGetStartChannel(adc), tmp->valueint );

    if ( ( tmp = cJSON_GetObjectItem(adcconfig,"oversample" ) ) )
        ADCConfigBlockSetOversample( adc, atoi(tmp->valuestring) );
    else 
        ADCConfigBlockSetOversample( adc, 0 );

    return adc;
}





#ifdef __cplusplus
}
#endif


#ifdef SELF_TEST

#include "gtest/gtest.h"
#include "tap.h"
#include "AIOUSBDevice.h"
using namespace AIOUSB;


TEST(ADCConfigBlock, YAMLRepresentation)
{
    ADCConfigBlock config;
    AIOUSBDevice dev;
    AIOUSBDeviceInitializeWithProductID( &dev, USB_AIO16_16A );

    ADCConfigBlockInitialize( &config, &dev );
    /* Some random configurations */

    /* set the channels 3-5 to be 0-2V */
    for ( unsigned channel = 3; channel <= 5; channel ++ )
        ADCConfigBlockSetGainCode( &config, channel, AD_GAIN_CODE_0_2V );
    
   
    /* Set the channels 6-9 to be +-5V */
    for ( unsigned channel = 6; channel <= 9; channel ++ )
        ADCConfigBlockSetGainCode( &config, channel, AD_GAIN_CODE_5V  );

    /* Set the number of oversamples to be 201 */
    ADCConfigBlockSetOversample( &config, 201 );
    /* Set external triggered */
    ADCConfigBlockSetScanRange( &config, 0, 15 );

    EXPECT_STREQ( ADCConfigBlockToYAML( &config ), "---\nadcconfig:\n  channels:\n    - gain: 0-10V\n    - gain: 0-10V\n    - gain: 0-10V\n    - gain: 0-2V\n    - gain: 0-2V\n    - gain: 0-2V\n    - gain: +-5V\n    - gain: +-5V\n    - gain: +-5V\n    - gain: 0-10V\n    - gain: 0-10V\n    - gain: 0-10V\n    - gain: 0-10V\n    - gain: 0-10V\n    - gain: 0-10V\n    - gain: 0-10V\n  calibration: Normal\n  trigger:\n     reference: sw\n     edge: rising-edge\n     refchannel: single-channel\n  start_channel: 0\n  end_channel: 15\n  oversample: 201\n" );
}

TEST(ADCConfigBlock, JSONRepresentation)
{
    ADCConfigBlock config;
    AIOUSBDevice dev;
    AIOUSBDeviceInitializeWithProductID( &dev, USB_AIO16_16A );

    ADCConfigBlockInitialize( &config, &dev );
    /* Some random configurations */

    /* set the channels 3-5 to be 0-2V */
    for ( unsigned channel = 3; channel <= 5; channel ++ )
        ADCConfigBlockSetGainCode( &config, channel, AD_GAIN_CODE_0_2V );
    
    /* Set the channels 6-9 to be +-5V */
    for ( unsigned channel = 6; channel <= 9; channel ++ )
        ADCConfigBlockSetGainCode( &config, channel, AD_GAIN_CODE_5V  );

    /* Set the number of oversamples to be 201 */
    ADCConfigBlockSetOversample( &config, 201 );
    /* Set external triggered */
    ADCConfigBlockSetScanRange( &config, 0, 15 );

    EXPECT_STREQ("{\"adcconfig\":{\"channels\":[{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-2V\"},{\"gain\":\"0-2V\"},{\"gain\":\"0-2V\"},{\"gain\":\"+-5V\"},{\"gain\":\"+-5V\"},{\"gain\":\"+-5V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"}],\"calibration\":\"Normal\",\"trigger\":{\"reference\":\"sw\",\"edge\":\"rising-edge\",\"refchannel\":\"single-channel\"},\"start_channel\":\"0\",\"end_channel\":\"15\",\"oversample\":\"201\"}}", 
                 ADCConfigBlockToJSON( &config ) );

    ADCConfigBlockSetOversample( &config, 102 );
    EXPECT_STREQ("{\"adcconfig\":{\"channels\":[{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-2V\"},{\"gain\":\"0-2V\"},{\"gain\":\"0-2V\"},{\"gain\":\"+-5V\"},{\"gain\":\"+-5V\"},{\"gain\":\"+-5V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"},{\"gain\":\"0-10V\"}],\"calibration\":\"Normal\",\"trigger\":{\"reference\":\"sw\",\"edge\":\"rising-edge\",\"refchannel\":\"single-channel\"},\"start_channel\":\"0\",\"end_channel\":\"15\",\"oversample\":\"102\"}}", 
                 ADCConfigBlockToJSON( &config ) );
    
}



TEST(ADCConfigBlock,CopyConfigs ) 
{
    ADCConfigBlock from,to;
    AIOUSBDevice dev;
    AIORET_TYPE result;

    AIOUSBDeviceInitializeWithProductID( &dev, USB_AIO16_16A );


    ADCConfigBlockInitialize( &from , &dev);
    ADCConfigBlockInitialize( &to , &dev );
   
    /* verify copying the test state */
    from.testing = AIOUSB_TRUE;
    result  = ADCConfigBlockCopy( &to, &from );
    
    EXPECT_GE( result, AIOUSB_SUCCESS );
    
    EXPECT_EQ( from.testing, to.testing );

    /* Change the register settings */
    for ( int i = 0; i < 16; i ++ )
        from.registers[i] = i % 3;
    ADCConfigBlockCopy( &to, &from );
    for ( int i = 0; i < 16; i ++ )
        EXPECT_EQ( from.registers[i], to.registers[i] );

}
TEST( ADCConfigBlock, CanSetDevice )
{
    ADCConfigBlock tmp;
    AIOUSBDevice device;

    ADCConfigBlockInitialize( &tmp , &device );
    ADCConfigBlockSetTesting( &tmp, AIOUSB_TRUE );
    ADCConfigBlockSetDevice( &tmp, &device   ) ;

    EXPECT_EQ( ADCConfigBlockGetAIOUSBDevice( &tmp, NULL ), &device );
}

int main(int argc, char *argv[] )
{
    testing::InitGoogleTest(&argc, argv);
    testing::TestEventListeners & listeners = testing::UnitTest::GetInstance()->listeners();
#ifdef GTEST_TAP_PRINT_TO_STDOUT
    delete listeners.Release(listeners.default_result_printer());
#endif

    listeners.Append( new tap::TapListener() );
    return RUN_ALL_TESTS();  
}
#endif
