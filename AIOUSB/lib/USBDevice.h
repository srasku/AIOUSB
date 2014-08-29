#ifndef _USB_DEVICE_H
#define _USB_DEVICE_H

#include <stdint.h>
#include <libusb.h>
#include <stdlib.h>
#include "ADCConfigBlock.h"

#ifdef __aiousb_cplusplus
namespace AIOUSB {
#endif

typedef struct aiousb_device { 

    int (*usb_control_transfer)( struct aiousb_device *usbdev, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout );

    int (*usb_bulk_transfer)( struct aiousb_device *dev_handle,
                      unsigned char endpoint, unsigned char *data, int length,
                      int *actual_length, unsigned int timeout );

    int (*usb_request)( struct aiousb_device *usbdev, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout );
    int (*usb_reset_device)(struct aiousb_device *usbdev );

    uint8_t timeout;
    libusb_device *device;
    libusb_device_handle *deviceHandle;
    struct libusb_device_descriptor deviceDesc;
    AIOUSB_BOOL debug;
} USBDevice;


USBDevice * NewUSBDevice(libusb_device *dev, libusb_device_handle *handle );
void DeleteUSBDevice( USBDevice *dev );
USBDevice *CopyUSBDevice( USBDevice *usb );
AIORET_TYPE InitializeUSBDevice( USBDevice *usb );

AIORET_TYPE FindUSBDevices( USBDevice **devs, int *size );
void DeleteUSBDevices( USBDevice *devs);

AIORET_TYPE USBDeviceClose( USBDevice *dev );


USBDevice *AIODeviceTableGetUSBDevice( unsigned long DeviceIndex , AIORESULT *result );
AIORET_TYPE USBDeviceReadADCConfigBlock( USBDevice *usb, ADCConfigBlock *configBlock );
AIORET_TYPE USBDeviceGetIdProduct( USBDevice *device );

AIORET_TYPE usb_control_transfer(struct aiousb_device *dev_handle,
                                 uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                                 unsigned char *data, uint16_t wLength, unsigned int timeout);
AIORET_TYPE usb_bulk_transfer(struct aiousb_device *dev_handle,
                              unsigned char endpoint, unsigned char *data, int length,
                              int *actual_length, unsigned int timeout);
AIORET_TYPE usb_request(struct aiousb_device *dev_handle,
                        uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex,
                        unsigned char *data, uint16_t wLength, unsigned int timeout);
AIORET_TYPE usb_reset_device( struct aiousb_device *usb );

 
libusb_device_handle *get_usb_device( struct aiousb_device *dev );


#ifdef __aiousb_cplusplus
}
#endif

#endif
