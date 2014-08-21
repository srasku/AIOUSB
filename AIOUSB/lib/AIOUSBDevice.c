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
void DeleteAIOUSBDevice( AIOUSBDevice *dev) 
{
    free(dev);
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE _verify_device( AIOUSBDevice *dev ) 
{
    AIORET_TYPE result = AIOUSB_SUCCESS;
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
    
    result = ADCConfigBlockCopyConfig( _get_config( dev ) , conf );
    return result;
}

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
AIORET_TYPE AIOUSBDeviceGetTesting( AIOUSBDevice *dev )
{
    AIORET_TYPE result = _verify_device( dev );
    if ( result != AIOUSB_SUCCESS )
        return -result;
    return dev->testing;
}

/*----------------------------------------------------------------------------*/
libusb_device_handle *AIOUSBDeviceGetUSBHandle( AIOUSBDevice *dev ) 
{
    return dev->deviceHandle;
}

/*----------------------------------------------------------------------------*/
libusb_device_handle *AIOUSBDeviceGetUSBHandleFromDeviceIndex( unsigned long DeviceIndex, AIOUSBDevice **dev, AIORESULT *result ) 
{
    
    *dev = AIODeviceTableGetDeviceAtIndex( DeviceIndex , result );
    libusb_device_handle *dh = NULL;
    if ( *result != AIOUSB_SUCCESS ) {
    } else if ( !*dev  ) {
        *result = AIOUSB_ERROR_DEVICE_NOT_FOUND;
    } else {
        dh = AIOUSBDeviceGetUSBHandle( *dev );
    }
    return dh;
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




TEST(Initialization, SetDifferentConfigBlocks ) 
{
    AIOUSBDevice *dev = NewAIOUSBDevice(0) ;
    AIORESULT result;
    EXPECT_TRUE( dev );
    ADCConfigBlock *readconf = NULL;
    ADCConfigBlock *conf = (ADCConfigBlock*)calloc(sizeof(ADCConfigBlock),1);
    EXPECT_TRUE(conf);
    memset(conf->registers,1,16);

    /* Note that ADCConfigBlock must be setup correctly, otherwise 
       certain tests on it will fail */
    result = AIOUSBDeviceSetADCConfigBlock( dev, conf );
    EXPECT_NE( result, AIOUSB_TRUE );

    result = ADCConfigBlockInitialize( conf ); 
    EXPECT_EQ( result, AIOUSB_SUCCESS );

    result = AIOUSBDeviceSetADCConfigBlock( dev, conf );
    EXPECT_EQ( result, AIOUSB_SUCCESS );

    readconf = AIOUSBDeviceGetADCConfigBlock( dev );
    for( int i = 0; i < 16; i ++ )
        EXPECT_EQ( conf->registers[i], readconf->registers[i] );

    free(conf);
    DeleteAIOUSBDevice( dev );
}

/**
 * @todo Want the API to set the isInit flag in case the device is ever added
 */
TEST(Initialization,AddingDeviceSetsInit )
{

    AIOUSBDevice *dev;
    
    

}

TEST(TestingFeatures, PropogateTesting ) 
{
    AIOUSBDevice *dev = NewAIOUSBDevice(0) ;
    ADCConfigBlock conf;
    ADCConfigBlockInitialize( &conf );
    
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
