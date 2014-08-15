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

