/**
 * @file   mock_capture_usb.c
 * @author Jimi Damon <james.damon@accesio.com>
 * @date   Tue Feb 17 12:01:40 2015
 * 
 * @brief  This file will allow capturing of all USB traffic, in and out
 * 
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include "AIOTypes.h"
#include "USBDevice.h"
#include "AIOUSB_Core.h"
#include "AIOUSB_Log.h"

#include <dlfcn.h>

typedef enum {
    IN,
    OUT
} IO_DIRECTION;


int (*orig_usb_control_transfer)( struct aiousb_device *usbdev, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout );

int (*orig_usb_bulk_transfer)( struct aiousb_device *dev_handle,
                  unsigned char endpoint, unsigned char *data, int length,
                  int *actual_length, unsigned int timeout );

int (*orig_usb_request)( struct aiousb_device *usbdev, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout );
int (*orig_usb_reset_device)(struct aiousb_device *usbdev );
int (*orig_usb_put_config)( struct aiousb_device *usb, ADCConfigBlock *configBlock );
int (*orig_usb_get_config)( struct aiousb_device *usb, ADCConfigBlock *configBlock );

FILE *outfile;


int mock_usb_control_transfer( struct aiousb_device *usbdev, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout )
{
    printf("Wrapping control\n");
    IO_DIRECTION direction;
    int retval;
    if ( request_type ==  USB_WRITE_TO_DEVICE ) {
        direction = OUT;
    } else if ( request_type == USB_READ_FROM_DEVICE ) {
        direction = IN;
    }
    retval = orig_usb_control_transfer( usbdev, request_type, bRequest, wValue, wIndex, data, wLength, timeout );
    fprintf(outfile,"%s,%s,%2.2x,%2.2x,%2.2x,%2.2x,", "control", (direction == IN ? "in" : "out" ),request_type, bRequest,wValue,wIndex );
    for( int i = 0; i < wLength; i ++ ) {
        fprintf(outfile,"%2.2x ", (unsigned short)data[i] );
    }
    fprintf(outfile,"\n");
    fflush(outfile);
    return retval;
}

int mock_usb_bulk_transfer( struct aiousb_device *dev_handle,
                            unsigned char endpoint, unsigned char *data, int length,
                            int *actual_length, unsigned int timeout )
{

    printf("Wrapping bulk ");
    IO_DIRECTION direction;
    int retval;

    if ( endpoint & LIBUSB_ENDPOINT_OUT ) {
        direction = OUT;
    } else if ( endpoint & LIBUSB_ENDPOINT_IN ) {
        direction = IN;
    }
    printf("(%s)\n", ( direction == IN ? "in" : "out" ));

    retval = orig_usb_bulk_transfer( dev_handle, endpoint, data, length, actual_length, timeout );
    fprintf(outfile,"%s,%s,%2.2x,%2.2x,%2.2x,", "bulk",(direction == IN ? "in" : "out" ), endpoint, length, *actual_length );
    for( int i = 0; i < *actual_length; i ++ ) {
        fprintf(outfile,"%2.2x ", (unsigned short)data[i] );
    }
    fprintf(outfile,"\n");

    return retval;
}

int mock_usb_request( struct aiousb_device *usbdev, uint8_t request_type, uint8_t bRequest, uint16_t wValue, uint16_t wIndex, unsigned char *data, uint16_t wLength, unsigned int timeout )
{
    printf("wrapping usb_request\n");
    return orig_usb_request( usbdev, request_type, bRequest, wValue, wIndex, data, wLength, timeout );
}

int mock_usb_reset_device(struct aiousb_device *usbdev )
{
    printf("Wrapping reset_device\n");
    return orig_usb_reset_device( usbdev );
}
 
int mock_usb_put_config( struct aiousb_device *usb, ADCConfigBlock *configBlock )
{
    printf("Wrapping put_config\n");
    int retval;
    IO_DIRECTION direction = OUT;

    retval = orig_usb_put_config( usb, configBlock );

    return retval;
}

int mock_usb_get_config( struct aiousb_device *usb, ADCConfigBlock *configBlock )
{
    printf("Wrapping get_config\n");
    return orig_usb_get_config( usb, configBlock );
}

/**
 * @brief Wraps the initial IntializeUSBDevice, and records mock
 * functions that will call the initial values.
 *
 */
AIOEither InitializeUSBDevice( USBDevice *usb, LIBUSBArgs *args )
{
    AIOEither retval = {0};
    static AIOEither (*init_usb_device)(USBDevice *usb, LIBUSBArgs *args ) = NULL;

    unsigned char *c;
    int port,ok=1;

    if (!init_usb_device) 
        init_usb_device = dlsym(RTLD_NEXT,"InitializeUSBDevice");

    printf("Wrapped the original !!\n");
    retval = init_usb_device( usb, args );

    outfile = fopen("usb_datalog.txt","w");
    if (!outfile ) {
        fprintf(stderr,"Can't open outputfile\n");
    }

    if ( !AIOEitherHasError( &retval ) ) {
        orig_usb_control_transfer  = usb->usb_control_transfer;
        orig_usb_bulk_transfer     = usb->usb_bulk_transfer;
        orig_usb_request           = usb->usb_request;
        orig_usb_reset_device      = usb->usb_reset_device;
        /* orig_usb_put_config        = usb->usb_put_config; */
        /* orig_usb_get_config        = usb->usb_get_config; */

        usb->usb_control_transfer  = mock_usb_control_transfer;
        usb->usb_bulk_transfer     = mock_usb_bulk_transfer;
        /* usb->usb_put_config        = mock_usb_put_config; */
        /* usb->usb_get_config        = mock_usb_get_config; */
        usb->usb_reset_device      = mock_usb_reset_device;
        usb->usb_request           = mock_usb_request;
    }

    printf("Done calling the original !!\n");

    return retval;
}


