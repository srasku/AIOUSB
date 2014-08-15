#include "AIODeviceTable.h" 

#ifdef __cplusplus
namespace AIOUSB {
#endif

/*----------------------------------------------------------------------------*/
AIOUSBDevice *_get_device( unsigned long index , AIORESULT *result )
{
    AIOUSBDevice *dev;
    if ( index > MAX_USB_DEVICES ) { 
        *result = AIOUSB_ERROR_INVALID_INDEX;
        return NULL;
    }
    dev = &deviceTable[index];
    if ( !dev ) {
        *result = AIOUSB_ERROR_INVALID_DATA;
        return NULL;
    }
    return dev;
}

/*----------------------------------------------------------------------------*/ 
AIOUSBDevice *AIODeviceTableGetDeviceAtIndex( unsigned long index , AIORESULT *result ) 
{
    *result = AIOUSB_Validate( &index );
    if ( *result != AIOUSB_SUCCESS ) 
        return NULL;

    return _get_device( index, result );
}


/*----------------------------------------------------------------------------*/
AIORESULT AIODeviceTableAddDeviceToDeviceTableWithUSBDevice( int *numAccesDevices, unsigned long productID , libusb_device *usb_dev ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = _get_device( *numAccesDevices , &result );
    device->device        = usb_dev;
    device->deviceHandle  = NULL;
    device->ProductID     = productID;
    /* device->isInit        = AIOUSB_TRUE; */
    /* ADCConfigBlockSetDevice( AIOUSBDeviceGetADCConfigBlock( device ), device ); */
    /* _setup_device_parameters( device , productID ); */
    *numAccesDevices += 1;
    return result;
}

#ifdef __cplusplus
}
#endif




