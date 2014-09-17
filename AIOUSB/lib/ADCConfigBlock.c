#include "ADCConfigBlock.h"
#include "AIOUSBDevice.h"
#include "AIOUSB_Core.h"

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

    config->device   = dev;
    config->size     = dev->ConfigBytes;
    config->testing  = dev->testing;
    config->timeout  = dev->commTimeout;

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

    unsigned gainCode = FIRST_ENUM(ADGainCode);             // return reasonable value on error
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
        if ( reg < AD_NUM_GAIN_CODES )
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
    AIOUSB_UnLock();
 return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADCConfigBlockSetCalMode(ADCConfigBlock *config, ADCalMode calMode)
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    if ( !config || !config->device || config->size == 0 )
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

#ifdef __cplusplus
}
#endif




#ifdef SELF_TEST

#include "gtest/gtest.h"
#include "tap.h"
#include "AIOUSBDevice.h"
using namespace AIOUSB;

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
