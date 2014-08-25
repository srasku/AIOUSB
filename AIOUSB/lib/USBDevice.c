/**
 * @file   USBDevice.c
 * @author  $Format: %an <%ae>$
 * @date   $Format: %ad$
 * @version $Format: %t$
 */

#include "AIOTypes.h"
#include "USBDevice.h"
#include "libusb.h"
#include "AIODeviceTable.h"

#ifdef __cplusplus
namespace AIOUSB {
#endif

/*----------------------------------------------------------------------------*/
USBDevice * NewUSBDevice( unsigned long DeviceIndex )
{
    USBDevice *obj = (USBDevice *)calloc(sizeof(USBDevice), 1 );
    if ( obj )  {
        AIORESULT result = AIOUSB_SUCCESS;
        obj->usb_control_transfer = usb_control_transfer;
        obj->usb_bulk_transfer = usb_bulk_transfer;
        obj->usb_request = usb_request;
        AIOUSBDevice *tmpdev = AIODeviceTableGetDeviceAtIndex( DeviceIndex , &result );
        if( tmpdev && result == AIOUSB_SUCCESS ) 
            obj->usb = tmpdev->deviceHandle;
    }
    return obj;
}

/*----------------------------------------------------------------------------*/
void DeleteUSBDevice( USBDevice *dev )
{
    free(dev);
}

/*----------------------------------------------------------------------------*/
USBDevice *AIODeviceTableGetUSBDevice( unsigned long DeviceIndex , AIORESULT *result ) 
{
    if ( DeviceIndex > MAX_USB_DEVICES ) {
        if ( result )
            *result = AIOUSB_ERROR_INVALID_PARAMETER;

        return NULL;
    }

    return NewUSBDevice( DeviceIndex );
}

/*----------------------------------------------------------------------------*/
libusb_device_handle *get_usb_device( struct aiousb_device *dev )
{
    if( !dev )
        return NULL;
    return dev->usb;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE USBDeviceReadADCConfigBlock( USBDevice *usb, ADCConfigBlock *configBlock )
{
    AIORET_TYPE result = AIOUSB_SUCCESS;

    ADCConfigBlock config;
    AIOUSB_UnLock();
    assert(configBlock);
    assert(usb);
    if ( !usb || !configBlock )
        return -AIOUSB_ERROR_INVALID_PARAMETER;

    ADCConfigBlockInitialize( &config );
    config.timeout = configBlock->timeout;

    AIOUSB_UnLock(); /* unlock while communicating with device */
    if( configBlock->testing != AIOUSB_TRUE ) {
        int bytesTransferred = usb->usb_control_transfer( usb, 
                                                          USB_READ_FROM_DEVICE,
                                                          AUR_ADC_GET_CONFIG,
                                                          0,
                                                          0,
                                                          config.registers,
                                                          config.size,
                                                          config.timeout
                                                          );
        
        if ( bytesTransferred != ( int ) config.size) {
            result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
            goto out_ReadConfigBlock;
        }
        /*
         * check and correct settings read from device
         */

        ADCConfigBlockCopy( configBlock, &config );
    }

 out_ReadConfigBlock:
    AIOUSB_UnLock();
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE USBDeviceWriteADCConfigBlock( USBDevice *usb, ADCConfigBlock *configBlock )
{
    AIORET_TYPE retval;
    assert(usb != NULL && configBlock != NULL );
    if ( !usb || !configBlock )
        return -AIOUSB_ERROR_INVALID_PARAMETER;

    if( configBlock->testing != AIOUSB_TRUE ) {
        int bytesTransferred = usb->usb_control_transfer( usb, 
                                                          USB_WRITE_TO_DEVICE,
                                                          AUR_ADC_SET_CONFIG,
                                                          0,
                                                          0,
                                                          configBlock->registers,
                                                          configBlock->size,
                                                          configBlock->timeout
                                                          );
        if ( bytesTransferred != (int)configBlock->size ) {
            
        } else {
            retval = bytesTransferred;
        }
    }
    return retval;
}

/*----------------------------------------------------------------------------*/
int usb_control_transfer(struct aiousb_device *dev_handle,
                         uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                         unsigned char *data, uint16_t wLength, unsigned int timeout)
{
    return libusb_control_transfer( get_usb_device( dev_handle ),
                                    request_type,
                                    bRequest,
                                    wValue,
                                    wIndex,
                                    data,
                                    wLength, 
                                    timeout
                                    );

}

/*----------------------------------------------------------------------------*/
int usb_bulk_transfer(struct aiousb_device *dev_handle,
                      unsigned char endpoint, unsigned char *data, int length,
                      int *actual_length, unsigned int timeout
                      )
{
    return libusb_bulk_transfer( get_usb_device( dev_handle ),
                                 endpoint,
                                 data,
                                 length,
                                 actual_length, 
                                 timeout
                                 );
}

int usb_request(struct aiousb_device *dev_handle,
                uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                unsigned char *data, uint16_t wLength, unsigned int timeout)
{
    return 1;
}


#ifdef __cplusplus
}
#endif

