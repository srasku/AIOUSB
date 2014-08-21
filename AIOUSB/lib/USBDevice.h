#ifndef _USB_DEVICE_H
#define _USB_DEVICE_H

#include <stdint.h>
#include <libusb.h>
#include <stdlib.h>

#ifdef __aiousb_cplusplus
namespace AIOUSB {
#endif

typedef struct aiousb_device { 

    int (*usb_control_transfer)( struct aiousb_device *usbdev, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout );

    int (*usb_bulk_transfer)( struct aiousb_device *dev_handle,
                      unsigned char endpoint, unsigned char *data, int length,
                      int *actual_length, unsigned int timeout );

    int (*usb_request)( struct aiousb_device *usbdev, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout );

    uint8_t timeout;
    libusb_device_handle *usb;
} USBDevice;


USBDevice * NewUSBDevice( unsigned long DeviceIndex );
void DeleteUSBDevice( USBDevice *dev );
USBDevice *AIODeviceTableGetUSBDevice( unsigned long DeviceIndex , AIORESULT *result );


int usb_control_transfer(struct aiousb_device *dev_handle,
                             uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                             unsigned char *data, uint16_t wLength, unsigned int timeout);
int usb_bulk_transfer(struct aiousb_device *dev_handle,
                      unsigned char endpoint, unsigned char *data, int length,
                      int *actual_length, unsigned int timeout);
int usb_request(struct aiousb_device *dev_handle,
                    uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                    unsigned char *data, uint16_t wLength, unsigned int timeout);

libusb_device_handle *get_usb_device( struct aiousb_device *dev );


#ifdef __aiousb_cplusplus
}
#endif

#endif
