#ifndef _USB_DEVICE_H
#define _USB_DEVICE_H

#include <stdint.h>
#include <libusb.h>
#include <stdlib.h>

#ifdef __aiousb_cplusplus
namespace AIOUSB {
#endif

/* typedef enum { AUR_DIO_CONFIG_QUERY, */
/*                AUR_DIO_CONFIG, */
/*                AUR_DIO_READ, */
/*                AUR_DIO_WRITE, */
/*                AUR_DIO_STREAM_OPEN_INPUT, */
/*                AUR_EEPROM_READ, */
/*                AUR_EEPROM_WRITE, */
/*                AUR_CTR_MODE, */
/*                AUR_ADC_GET_CONFIG, */
/*                AUR_ADC_SET_CONFIG, */
/*                AUR_START_ACQUIRING_BLOCK, */
/*                AUR_ADC_IMMEDIATE, */
/*                AUR_LOAD_BULK_CALIBRATION_BLOCK */
/* } USB_REQUEST; */

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
