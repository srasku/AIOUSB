#include "AIOUSBDevice.h"
#include "AIODeviceTable.h"
#include "AIOUSB_Core.h"


#ifdef __cplusplus
namespace AIOUSB {
#endif

/*----------------------------------------------------------------------------*/
AIOUSBDevice *NewAIOUSBDevice( unsigned long DeviceIndex ) 
{
    AIOUSBDevice *dev = (AIOUSBDevice *)calloc(sizeof(AIOUSBDevice),1);
    dev->deviceIndex = DeviceIndex;
    dev->isInit      = AIOUSB_TRUE;
    return dev;
}

/*----------------------------------------------------------------------------*/
AIOUSBDevice *NewAIOUSBDeviceFromJSON( char *str )
{
    AIOUSBDevice *dev = (AIOUSBDevice *)calloc(sizeof(AIOUSBDevice),1);
    cJSON *json;
    json = cJSON_Parse(str);

    if (!json ) 
        return NULL;
    cJSON *aiousbdevice = cJSON_GetObjectItem(json,"aiousbdevice");

    if (!aiousbdevice )
        goto err_NewAIOUSBDeviceFromJSON;

    cJSON *tmp;

    if ( (tmp = cJSON_GetObjectItem(aiousbdevice,"device_index") ) ) {
        dev->deviceIndex = cJSON_AsInteger( tmp );
    }
    if ( (tmp = cJSON_GetObjectItem(aiousbdevice,"adc_channels_per_group") ) ) {
        dev->ADCChannels = cJSON_AsInteger( tmp );
    }
    if ( (tmp = cJSON_GetObjectItem(aiousbdevice,"adc_mux_channels") ) ) {
        dev->ADCMUXChannels = cJSON_AsInteger( tmp );
    }
    if ( (tmp = cJSON_GetObjectItem(aiousbdevice,"adcconfig") ) ) {
        ADCConfigBlock *tmpconfig = NewADCConfigBlockFromJSON( tmp->valuestring );
        ADCConfigBlockCopy( AIOUSBDeviceGetADCConfigBlock( dev ), tmpconfig );
        DeleteADCConfigBlock(tmpconfig);
    }

    return dev;

 err_NewAIOUSBDeviceFromJSON:
    free(dev);
    return NULL;
}

/*----------------------------------------------------------------------------*/
char *AIOUSBDeviceToJSON( AIOUSBDevice *device )
{
    char tmpbuf[2048] = {0};
    /* \"device_index\":\"0\",\"adc_channels_per_group\":\"1\",\"adc_mux_channels\":\"16\",\ */
    sprintf( &tmpbuf[strlen(tmpbuf)], "{\"%s\":%d,", "device_index", device->deviceIndex );
    sprintf( &tmpbuf[strlen(tmpbuf)],  "\"%s\":%d,", "adc_channels_per_group", device->ADCChannels );
    sprintf( &tmpbuf[strlen(tmpbuf)],  "\"%s\":%d,", "adc_mux_channels", device->ADCMUXChannels );
    sprintf( &tmpbuf[strlen(tmpbuf)],  "\"%s\":%s", "adcconfig", ADCConfigBlockToJSON( AIOUSBDeviceGetADCConfigBlock( device )));
    strcat( tmpbuf, "}" );

    return strdup(tmpbuf);    
}

/*----------------------------------------------------------------------------*/
void DeleteAIOUSBDevice( AIOUSBDevice *dev) 
{
    free(dev);
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceInitializeWithProductID( AIOUSBDevice *device , ProductIDS productID )
{
    assert(device);
    if ( !device ) 
        return -AIOUSB_ERROR_INVALID_DEVICE;

    device->usb_device    = NULL;
    device->ProductID     = productID;
    device->isInit        = AIOUSB_TRUE;
    device->testing       = AIOUSB_FALSE;
    device->commTimeout   = 1000; /* Should be default value */
    _setup_device_parameters( device , productID );
    ADCConfigBlockSetDevice( AIOUSBDeviceGetADCConfigBlock( device ), device );

    return AIOUSB_SUCCESS;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceSetTimeout( AIOUSBDevice *device, unsigned timeout )
{
    assert(device);
    if (!device )
        return -AIOUSB_ERROR_INVALID_DEVICE;
    device->commTimeout = timeout;
    return AIOUSB_SUCCESS;
}
/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceGetTimeout( AIOUSBDevice *device )
{
    assert(device);
    if (!device )
        return -AIOUSB_ERROR_INVALID_DEVICE;
    return device->commTimeout;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceCopyADCConfigBlock( AIOUSBDevice *dev, ADCConfigBlock *newone )
{
    assert( dev && newone );
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    if ( !dev  ) 
        return AIOUSB_ERROR_INVALID_DEVICE;

    if ( !newone ) 
        return AIOUSB_ERROR_INVALID_ADCCONFIG;

    retval = ADCConfigBlockCopy( AIOUSBDeviceGetADCConfigBlock( dev ), newone );
    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE _verify_device( AIOUSBDevice *dev ) 
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    if ( !dev ) 
        return AIOUSB_ERROR_INVALID_DEVICE;
    if ( dev->isInit != AIOUSB_TRUE )
        return AIOUSB_ERROR_INVALID_DATA;

    if ( dev->deviceIndex < 0 || dev->deviceIndex > MAX_USB_DEVICES ) 
        return AIOUSB_ERROR_INVALID_INDEX;
    /* other checks */
    return result;
}

/*----------------------------------------------------------------------------*/
ADCConfigBlock *_get_config( AIOUSBDevice *dev )
{
    return &dev->cachedConfigBlock;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceSetADCConfigBlock( AIOUSBDevice *dev, ADCConfigBlock *conf )
{
    AIORET_TYPE result = _verify_device( dev );
    ADCConfigBlock *cb = AIOUSBDeviceGetADCConfigBlock( dev );
    if( !cb ) {
        return -AIOUSB_ERROR_DEVICE_NOT_FOUND;
    } else if (result != AIOUSB_SUCCESS ) 
        return result;
    
    result = ADCConfigBlockCopy( _get_config( dev ) , conf );
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceSize() 
{
    return sizeof(AIOUSBDevice);
}

/*----------------------------------------------------------------------------*/
ADCConfigBlock * AIOUSBDeviceGetADCConfigBlock( AIOUSBDevice *dev ) 
{
    return _get_config( dev );
}


/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceSetTesting( AIOUSBDevice *dev, AIOUSB_BOOL testing )
{

    ADCConfigBlock *conf;
    AIORET_TYPE result = _verify_device( dev );
    if ( result != AIOUSB_SUCCESS )
        return -result;
   
    dev->testing = testing;
    conf = AIOUSBDeviceGetADCConfigBlock( dev );
    if (!conf ) 
        return -AIOUSB_ERROR_INVALID_DATA;
    
    ADCConfigBlockSetTesting( conf, testing );

    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceGetStreamingBlockSize( AIOUSBDevice *dev )
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
    assert( dev );
    if (!dev )
        return -AIOUSB_ERROR_INVALID_DEVICE_SETTING;
    
    if (dev->bADCStream || dev->bDIOStream)
        result = dev->StreamingBlockSize;
    else
        result = -AIOUSB_ERROR_NOT_SUPPORTED;

    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceGetTesting( AIOUSBDevice *dev )
{
    AIORET_TYPE result = _verify_device( dev );
    if ( result != AIOUSB_SUCCESS )
        return -result;
    return dev->testing;
}

/*----------------------------------------------------------------------------*/
USBDevice *AIOUSBDeviceGetUSBHandle( AIOUSBDevice *dev ) 
{
    if ( !dev ) 
        return NULL;
    return dev->usb_device;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceSetUSBHandle( AIOUSBDevice *dev, USBDevice *usb )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    assert(dev);
    if ( !dev ) {
        retval =  -AIOUSB_ERROR_INVALID_DEVICE;
    } else if (!usb ) { 
        retval = -AIOUSB_ERROR_INVALID_PARAMETER;
    } else {
        dev->usb_device = usb;
    }
    return retval;

}

/*----------------------------------------------------------------------------*/
USBDevice *AIOUSBDeviceGetUSBHandleFromDeviceIndex( unsigned long DeviceIndex, AIOUSBDevice **dev, AIORESULT *result ) 
{
    
    *dev = AIODeviceTableGetDeviceAtIndex( DeviceIndex , result );
    USBDevice *dh;

    if ( *result != AIOUSB_SUCCESS ) {
    } else if ( !*dev  ) {
        *result = AIOUSB_ERROR_DEVICE_NOT_FOUND;
    } else {
        dh = AIOUSBDeviceGetUSBHandle( *dev );
    }
    return dh;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceGetDiscardFirstSample( AIOUSBDevice *device )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    assert( device );
    if ( !device )
        return -AIOUSB_ERROR_INVALID_DEVICE;

    retval = (AIORET_TYPE)device->discardFirstSample;

    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSBDeviceSetDiscardFirstSample( AIOUSBDevice *device , AIOUSB_BOOL discard )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    assert( device );
    if ( !device )
        return -AIOUSB_ERROR_INVALID_DEVICE;

    device->discardFirstSample = discard;

    return retval;
}


#ifdef __cplusplus
}
#endif

#ifdef SELF_TEST

#include <unistd.h>
#include <stdio.h>
#include "gtest/gtest.h"
#include "tap.h"
using namespace AIOUSB;

/**
 * @todo Want the API to set the isInit flag in case the device is ever added
 */
TEST(Initialization,AddingDeviceSetsInit )
{
    AIOUSBDevice dev;
    AIOUSBDeviceInitializeWithProductID( &dev , USB_AIO16_16A );

    EXPECT_EQ( dev.ProductID, USB_AIO16_16A );

    EXPECT_EQ( dev.isInit, AIOUSB_TRUE );
}


TEST(Initialization, SetDifferentConfigBlocks ) 
{
    AIOUSBDevice *dev = NewAIOUSBDevice(0) ;
    AIORET_TYPE result;
    EXPECT_TRUE( dev );
    ADCConfigBlock *readconf = NULL;
    ADCConfigBlock *conf = (ADCConfigBlock*)calloc(sizeof(ADCConfigBlock),1);

    AIOUSBDeviceInitializeWithProductID( dev , USB_AIO16_16A );

    EXPECT_TRUE(conf);
    ADCConfigBlockInitializeFromAIOUSBDevice( conf, dev );
    
    EXPECT_GE( conf->size, AD_CONFIG_REGISTERS ) << "Should be initialized to correct value " << (int)AD_CONFIG_REGISTERS << "\n";

    memset(conf->registers,1,16);

    /* Note that ADCConfigBlock must be setup correctly, otherwise 
       certain tests on it will fail */
    result = AIOUSBDeviceSetADCConfigBlock( dev, conf );
    EXPECT_NE( result, AIOUSB_TRUE );

    result = ADCConfigBlockInitializeFromAIOUSBDevice( conf , dev ); 
    EXPECT_EQ( result, AIOUSB_SUCCESS );

    result = AIOUSBDeviceSetADCConfigBlock( dev, conf );
    EXPECT_EQ( result, AIOUSB_SUCCESS );

    readconf = AIOUSBDeviceGetADCConfigBlock( dev );
    for( int i = 0; i < 16; i ++ )
        EXPECT_EQ( conf->registers[i], readconf->registers[i] );

    free(conf);
    DeleteAIOUSBDevice( dev );
}


TEST(TestingFeatures, PropogateTesting ) 
{
    AIOUSBDevice *dev = NewAIOUSBDevice(0) ;
    ADCConfigBlock conf;
    ADCConfigBlockInitializeFromAIOUSBDevice( &conf , dev );
    
    AIOUSBDeviceSetADCConfigBlock( dev, &conf );
    
    AIOUSBDeviceSetTesting( dev, AIOUSB_TRUE );
    
    EXPECT_NE( ADCConfigBlockGetTesting(AIOUSBDeviceGetADCConfigBlock(dev)), ADCConfigBlockGetTesting( &conf ));
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
