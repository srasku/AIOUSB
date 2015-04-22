/**
 * @file   AIOUSB_DIO.c
 * @author $Format: %an <%ae>$
 * @date   $Format: %ad$
 * @version $Format: %h$
 * @brief Core code to interface with Digital cards
 */

#include "AIOUSB_DIO.h"
#include "AIODeviceTable.h"
#include "AIOUSB_Core.h"
#include "USBDevice.h"

#ifdef __cplusplus
namespace AIOUSB {
#endif

#define MASK_BYTES_SIZE(device)  ((device->DIOBytes + BITS_PER_BYTE - 1) / BITS_PER_BYTE )
#define TRISTATE_BYTES_SIZE(device) ((device->Tristates + BITS_PER_BYTE - 1) / BITS_PER_BYTE)

/*----------------------------------------------------------------------------*/
/**
 * @desc Returns the number in Big-Endian format
 */
unsigned short aiousb_htons(unsigned short octaveOffset)
{
  short a;
  char tmp1, tmp2;
  a = 0xff;
  tmp1 = *((char*)&a); 
  if ( tmp1 == 0xff ) {	/* Little Endinan */
    a = octaveOffset;
    tmp1 = *((char*)&a); 
    tmp2 = *(((char*)&a)+1);
    *((char*)&a) = tmp2;
    *(((char*)&a)+1) = tmp1;
  }
  return a;
}


/*----------------------------------------------------------------------------*/
static unsigned short OctaveDacFromFreq(double *Hz) 
{
    assert(Hz != 0);
    unsigned short octaveOffset = 0;
    if(*Hz > 0) {
          if(*Hz > 40000000.0)
              *Hz = 40000000.0;
          int offset,
              octave = ( int )floor(3.322 * log10(*Hz / 1039));
          if(octave < 0) {
                octave = offset = 0;
            } else {
                offset = ( int )round(2048.0 - (ldexp(2078, 10 + octave) / *Hz));
                octaveOffset = (( unsigned short )octave << 12) | (( unsigned short )offset << 2);
                octaveOffset = aiousb_htons(octaveOffset); // oscillator wants the value in big-endian format
            }
          *Hz = (2078 << octave) / (2.0 - offset / 1024.0);
      }
    return octaveOffset;
}

/*----------------------------------------------------------------------------*/
AIOUSBDevice *_check_dio( unsigned long DeviceIndex, AIORESULT *result ) 
{
    AIOUSBDevice *device = AIODeviceTableGetDeviceAtIndex( DeviceIndex, result );

    if ( *result != AIOUSB_SUCCESS || !device ) {
        return NULL;
    } 

    if ( device->DIOBytes == 0) {
        *result = AIOUSB_ERROR_NOT_SUPPORTED;
        return NULL;
    }

    return device;
}

/*----------------------------------------------------------------------------*/
USBDevice *_check_dio_get_device_handle( unsigned long DeviceIndex, 
                                         AIOUSBDevice **device,  
                                         AIORESULT *result ) 
{
    USBDevice *deviceHandle = NULL;
    *device = _check_dio( DeviceIndex, result );
    if( *result != AIOUSB_SUCCESS ) {
        return deviceHandle;
    }

    return AIODeviceTableGetUSBDeviceAtIndex( DeviceIndex , result );
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_Configure(
                        unsigned long DeviceIndex,
                        unsigned char bTristate,
                        AIOChannelMask *mask,
                        DIOBuf *buf
                        ) 
{
    AIOUSBDevice  *device = NULL;
    AIORESULT result;
    char *dest, *configBuffer;
    int bufferSize;
    int bytesTransferred;

    if ( !mask  || !buf  || ( bTristate != AIOUSB_FALSE && bTristate != AIOUSB_TRUE ) )
        return AIOUSB_ERROR_INVALID_PARAMETER;

    USBDevice *deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device, &result );
    if ( device->LastDIOData == 0 )
        return AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    memcpy(device->LastDIOData, DIOBufToBinary(buf), DIOBufByteSize( buf ) );
    bufferSize = device->DIOBytes + MASK_BYTES_SIZE(device);
    configBuffer = ( char* )malloc( bufferSize );
    if ( !configBuffer ) {
        return AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
    }

    dest = configBuffer;

    memcpy( dest, DIOBufToBinary(buf), device->DIOBytes );
    dest += device->DIOBytes;

    for( unsigned i = 0; i < MASK_BYTES_SIZE( device ) ; i ++ ) {
        char tmpmask;
        if ( (result = AIOChannelMaskGetMaskAtIndex( mask, &tmpmask, i ) != AIOUSB_SUCCESS ) ) { 
            return result;
        }
        memcpy( dest, &tmpmask, 1 );
        dest += 1;
    }

    bytesTransferred = deviceHandle->usb_control_transfer(deviceHandle,
                                                          USB_WRITE_TO_DEVICE,
                                                          AUR_DIO_CONFIG,
                                                          bTristate,
                                                          0, 
                                                          (unsigned char *)configBuffer,
                                                          bufferSize,
                                                          device->commTimeout 
                                                          );


    if (bytesTransferred != bufferSize )
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
    free(configBuffer);

    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_ConfigureRaw(
                           unsigned long DeviceIndex,
                           unsigned char bTristate,
                           void *pOutMask,
                           void *pData
                           ) 
{
    if( !pOutMask  || !pData  || ( bTristate != AIOUSB_FALSE && bTristate != AIOUSB_TRUE ))
        return AIOUSB_ERROR_INVALID_PARAMETER;

    AIORESULT result ;
    AIOUSBDevice *device = _check_dio( DeviceIndex, &result );

    if ( device->LastDIOData != 0 ) 
        return AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    memcpy(device->LastDIOData, pData, device->DIOBytes);

    USBDevice *deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device, &result );

    if ( !deviceHandle ) {
        return AIOUSB_ERROR_DEVICE_NOT_CONNECTED;
    }
    
    int bufferSize = device->DIOBytes + 2 * MASK_BYTES_SIZE( device );

    unsigned char *configBuffer = ( unsigned char* )malloc(bufferSize);

    if (!configBuffer )
        return AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    unsigned char *dest = configBuffer;
    memcpy(dest, pData, device->DIOBytes);
    dest += device->DIOBytes;
    memcpy(dest, pOutMask, MASK_BYTES_SIZE( device ));
    dest += MASK_BYTES_SIZE( device );
    memset(dest, 0, MASK_BYTES_SIZE( device ) );
    
    int bytesTransferred = deviceHandle->usb_control_transfer(deviceHandle,
                                                              USB_WRITE_TO_DEVICE,
                                                              AUR_DIO_CONFIG,
                                                              bTristate,
                                                              0, 
                                                              (unsigned char *)configBuffer,
                                                              bufferSize,
                                                              device->commTimeout 
                                                              );

    if(bytesTransferred != bufferSize)
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);

    free(configBuffer);
    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_ConfigureEx( 
                          unsigned long DeviceIndex, 
                          void *pOutMask, 
                          void *pData, 
                          void *pTristateMask 
                           ) 
{ 
    if( !pOutMask  || !pData || ! pTristateMask )
        return AIOUSB_ERROR_INVALID_PARAMETER;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = NULL;
    USBDevice * deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device, &result );

    if ( !deviceHandle ) {
        return AIOUSB_ERROR_DEVICE_NOT_CONNECTED;
    }

    memcpy(device->LastDIOData, pData, device->DIOBytes);

    int bufferSize = device->DIOBytes + MASK_BYTES_SIZE( device) + TRISTATE_BYTES_SIZE(device);
    unsigned char *configBuffer = ( unsigned char* )malloc(bufferSize);

    if (!configBuffer )
        return AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    unsigned char *dest = configBuffer;
    memcpy(dest, pData, device->DIOBytes);
    dest += device->DIOBytes;
    memcpy(dest, pOutMask, MASK_BYTES_SIZE( device ) );
    dest += MASK_BYTES_SIZE( device );
    memcpy(dest, pTristateMask, TRISTATE_BYTES_SIZE( device ) );

    int bytesTransferred = deviceHandle->usb_control_transfer(deviceHandle,
                                                              USB_WRITE_TO_DEVICE,
                                                              AUR_DIO_CONFIG,
                                                              0,
                                                              device->DIOBytes,
                                                              configBuffer,
                                                              bufferSize,
                                                              device->commTimeout
                                                              );

    if(bytesTransferred != bufferSize)
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
    free(configBuffer);

    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_ConfigurationQuery(
                                 unsigned long DeviceIndex,
                                 void *pOutMask,
                                 void *pTristateMask
                                 ) 
{
    if( !pOutMask || pTristateMask == NULL )
        return AIOUSB_ERROR_INVALID_PARAMETER;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = _check_dio( DeviceIndex, &result );    
    USBDevice *deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device, &result );

    if (!deviceHandle ) {
        return AIOUSB_ERROR_DEVICE_NOT_FOUND;
    }

    int bufferSize = MASK_BYTES_SIZE( device ) + TRISTATE_BYTES_SIZE( device );
    unsigned char *configBuffer = ( unsigned char* )malloc(bufferSize);

    if ( !configBuffer ) 
        return AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    int bytesTransferred = deviceHandle->usb_control_transfer(deviceHandle,
                                                              USB_READ_FROM_DEVICE,
                                                              AUR_DIO_CONFIG_QUERY,
                                                              0,
                                                              device->DIOBytes,
                                                              configBuffer,
                                                              bufferSize,
                                                              device->commTimeout
                                                              );


    if ( bytesTransferred == bufferSize ) {
        memcpy(pOutMask, configBuffer, MASK_BYTES_SIZE( device ) );
        memcpy(pTristateMask, configBuffer + MASK_BYTES_SIZE( device ), TRISTATE_BYTES_SIZE( device ) );
    } else {
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
    }
    free(configBuffer);
    
    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_WriteAll(
                       unsigned long DeviceIndex,
                       void *pData
                       ) 
{
    if( !pData )
        return AIOUSB_ERROR_INVALID_PARAMETER;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = NULL;
    USBDevice *deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device, &result );

    if (!deviceHandle ) {
        return AIOUSB_ERROR_DEVICE_NOT_CONNECTED;
    }

    if (device->LastDIOData == 0) {
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
    }

    char foo[10] = {};
    memcpy(foo, pData, device->DIOBytes);
    memcpy(device->LastDIOData, pData, device->DIOBytes);

    int bytesTransferred = deviceHandle->usb_control_transfer(deviceHandle,
                                                              USB_WRITE_TO_DEVICE,
                                                              AUR_DIO_WRITE,
                                                              0,
                                                              0,
                                                              ( unsigned char* )pData,
                                                              device->DIOBytes,
                                                              device->commTimeout
                                                              );


    if(bytesTransferred != (signed)device->DIOBytes )
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);

    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_Write8(
                     unsigned long DeviceIndex,
                     unsigned long ByteIndex,
                     unsigned char Data
                     ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = NULL;
    USBDevice *deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device, &result );

    if ( !device->DIOBytes )
        return AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    if (!deviceHandle  )
        return AIOUSB_ERROR_DEVICE_NOT_CONNECTED;

    int dioBytes = device->DIOBytes;


    unsigned char * dataBuffer = ( unsigned char* )malloc(dioBytes);


    if (!dataBuffer )
        return AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    device->LastDIOData[ ByteIndex ] = Data;
    memcpy(dataBuffer, device->LastDIOData, dioBytes);
    
    int bytesTransferred = deviceHandle->usb_control_transfer(deviceHandle,
                                                              USB_WRITE_TO_DEVICE,
                                                              AUR_DIO_WRITE,
                                                              0,
                                                              0,
                                                              dataBuffer,
                                                              dioBytes,
                                                              device->commTimeout
                                                              );
    if(bytesTransferred != dioBytes)
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
    free(dataBuffer);

    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_Write1(
                     unsigned long DeviceIndex,
                     unsigned long BitIndex,
                     unsigned char bData
                     ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = _check_dio( DeviceIndex, &result );

    if ( result != AIOUSB_SUCCESS )
        return result;

    unsigned byteIndex = BitIndex / BITS_PER_BYTE;
    if(( bData != AIOUSB_FALSE && bData != AIOUSB_TRUE ) || byteIndex >= device->DIOBytes ) {
        return AIOUSB_ERROR_INVALID_PARAMETER;
    }

    if ( !device->LastDIOData ) {
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
    }
        
    unsigned char value = device->LastDIOData[ byteIndex ];
    unsigned char bitMask = 1 << (BitIndex % BITS_PER_BYTE);
    if(bData == AIOUSB_FALSE)
        value &= ~bitMask;
    else
        value |= bitMask;
    
    result = DIO_Write8(DeviceIndex, byteIndex, value);
    
    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_ReadAll(
                      unsigned long DeviceIndex,
                      void *buf
                      ) 
{
    if ( !buf ) 
        return AIOUSB_ERROR_INVALID_PARAMETER;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = NULL;
    USBDevice *usb = _check_dio_get_device_handle( DeviceIndex, &device,  &result );
    if ( !usb || result != AIOUSB_SUCCESS )
        return AIOUSB_ERROR_DEVICE_NOT_FOUND;

    int bytesTransferred = usb->usb_control_transfer( usb,
                                                      USB_READ_FROM_DEVICE, 
                                                      AUR_DIO_READ,
                                                      0,
                                                      0,
                                                      (unsigned char *)buf,
                                                      device->DIOBytes,
                                                      device->commTimeout
                                                      );
    
    if( bytesTransferred != (int)device->DIOBytes )
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT( bytesTransferred );
    
    return result;
}


/*----------------------------------------------------------------------------*/
AIORESULT DIO_ReadIntoDIOBuf(
                      unsigned long DeviceIndex,
                      DIOBuf *buf
                      ) 
{

    if ( !buf )
        return AIOUSB_ERROR_INVALID_PARAMETER;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = NULL;
    int bytesTransferred;
    char *tmpbuf;

    USBDevice *deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device,  &result );
    if ( !deviceHandle )
        return AIOUSB_ERROR_DEVICE_NOT_FOUND;
  
    tmpbuf = (char*)malloc( sizeof(char)*device->DIOBytes );

    if ( !tmpbuf )
        return AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
    
    bytesTransferred = deviceHandle->usb_control_transfer(deviceHandle,
                                                          USB_READ_FROM_DEVICE, 
                                                          AUR_DIO_READ,
                                                          0, 
                                                          0, 
                                                          (unsigned char *)tmpbuf,
                                                          device->DIOBytes,
                                                          device->commTimeout
                                                          );

    if( bytesTransferred < 0 || bytesTransferred != (int)device->DIOBytes ) {
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
        goto cleanup_DIO_ReadAll;
    }

    if ( DIOBufResize( buf, device->DIOBytes ) == NULL ) {
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
        goto cleanup_DIO_ReadAll;
    }
    
    DIOBufReplaceString( buf, tmpbuf, device->DIOBytes ); /* Copy to the DIOBuf */
    cleanup_DIO_ReadAll:
    free(tmpbuf);

    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_ReadAllToCharStr(
                               unsigned long DeviceIndex,
                               char *buf,
                               unsigned size
                               ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = NULL;
    USBDevice *deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device,  &result );
    if ( !deviceHandle ) {
        return result;
    }
    int bytes_to_transfer = MIN( size, device->DIOBytes );

    int bytesTransferred = deviceHandle->usb_control_transfer(deviceHandle,
                                                              USB_READ_FROM_DEVICE, 
                                                              AUR_DIO_READ,
                                                              0, 
                                                              0, 
                                                              (unsigned char *)buf,
                                                              bytes_to_transfer,
                                                              device->commTimeout
                                                              );
    if( bytesTransferred < 0 || bytesTransferred != (int)device->DIOBytes )
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
    
    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_Read8(
                    unsigned long DeviceIndex,
                    unsigned long ByteIndex,
                    int *pdat
                    ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = NULL;
    DIOBuf *readBuffer;
    if ( !_check_dio_get_device_handle( DeviceIndex, &device,  &result ) || result != AIOUSB_SUCCESS ) {
        goto out_DIO_Read8;
    }

    readBuffer = NewDIOBuf( device->DIOBytes );
    if ( !readBuffer ) {
        result =  AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
        goto out_DIO_Read8;
    }
    
    if ( (result = DIO_ReadAll(DeviceIndex, readBuffer)) == AIOUSB_SUCCESS ) {
        char *tmp = DIOBufToBinary( readBuffer ); 
        *pdat = (int)tmp[ByteIndex];
    }

    DeleteDIOBuf( readBuffer );

 out_DIO_Read8:

    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_Read1(
                    unsigned long DeviceIndex,
                    unsigned long BitIndex,
                    int *bit
                    ) 
{
    char result = AIOUSB_SUCCESS;
    int value = 0;
    if((result = DIO_Read8(DeviceIndex, BitIndex / BITS_PER_BYTE, &value )) >= AIOUSB_SUCCESS) {
         unsigned char bitMask = 1 << (( int )BitIndex % BITS_PER_BYTE);
        if((value & bitMask) != 0)
          *bit = AIOUSB_TRUE;
        else
          *bit = AIOUSB_FALSE;
    }
    return result;
}

/*----------------------------------------------------------------------------*/
AIOUSBDevice *_check_dio_stream( unsigned long DeviceIndex , AIORESULT *result ) 
{
    AIOUSBDevice *device = _check_dio( DeviceIndex, result );

    if(device->bDIOStream == AIOUSB_FALSE) {
        *result = AIOUSB_ERROR_NOT_SUPPORTED;
        return NULL;
    }
    if(device->bDIOOpen) {
        *result = AIOUSB_ERROR_OPEN_FAILED;
        return NULL;
    }
    return device;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_StreamOpen(
                         unsigned long DeviceIndex,
                         unsigned long bIsRead
                         ) 
{
    AIORESULT result;
    AIOUSBDevice *device = NULL;
    USBDevice *deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device, &result );
    if (! deviceHandle ) 
        return AIOUSB_ERROR_DEVICE_NOT_CONNECTED;

    int bytesTransferred = deviceHandle->usb_control_transfer(deviceHandle,
                                                              USB_WRITE_TO_DEVICE,
                                                              bIsRead ? AUR_DIO_STREAM_OPEN_INPUT : AUR_DIO_STREAM_OPEN_OUTPUT,
                                                              0, 
                                                              0, 
                                                              0, 
                                                              0, 
                                                              device->commTimeout
                                                              );
    if(bytesTransferred == 0) {
        device->bDIOOpen = AIOUSB_TRUE;
        device->bDIORead = bIsRead ? AIOUSB_TRUE : AIOUSB_FALSE;
    } else {
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
    }

    return result;
}


/*----------------------------------------------------------------------------*/
AIORESULT DIO_StreamClose(
                          unsigned long DeviceIndex
                          ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device =  _check_dio_stream( DeviceIndex, &result );
    if (result == AIOUSB_SUCCESS ) 
        device->bDIOOpen = AIOUSB_FALSE;

    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_StreamSetClocks(
                              unsigned long DeviceIndex,
                              double *ReadClockHz,
                              double *WriteClockHz
                              ) 
{
    if( *ReadClockHz < 0 || *WriteClockHz < 0  )
        return AIOUSB_ERROR_INVALID_PARAMETER;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = NULL;
    int CONFIG_BLOCK_SIZE = 5;
    unsigned char configBlock[ CONFIG_BLOCK_SIZE ];
    int bytesTransferred;
    USBDevice *usb = AIOUSBDeviceGetUSBHandleFromDeviceIndex( DeviceIndex, &device, &result );
    if ( result != AIOUSB_SUCCESS )
        goto out_DIO_StreamSetClocks;
    if (!usb ) {
        result = AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
        goto out_DIO_StreamSetClocks;
    }
    

    /**
     * @note  
     * @verbatim
     * fill in data for the vendor request
     * byte 0 used enable/disable read and write clocks
     *   bit 0 is write clock
     *   bit 1 is read  clock
     *     1 = off/disable
     *     0 = enable (1000 Khz is default value whenever enabled)
     * bytes 1-2 = write clock value
     * bytes 3-4 = read clock value
     * @endverbatim
     */


    configBlock[ 0 ] = 0x03; /* disable read and write clocks by default */

    if(*WriteClockHz > 0)
        configBlock[ 0 ] &= ~0x01; /* enable write clock */

    if(*ReadClockHz > 0)
        configBlock[ 0 ] &= ~0x02; /* enable read clock */

    *( unsigned short* )&configBlock[ 1 ] = OctaveDacFromFreq(WriteClockHz);
    *( unsigned short* )&configBlock[ 3 ] = OctaveDacFromFreq(ReadClockHz);
    bytesTransferred = usb->usb_control_transfer(usb,
                                                 USB_WRITE_TO_DEVICE, 
                                                 AUR_DIO_SETCLOCKS,
                                                 0, 
                                                 0, 
                                                 configBlock, 
                                                 CONFIG_BLOCK_SIZE, 
                                                 device->commTimeout
                                                 );
    if(bytesTransferred != CONFIG_BLOCK_SIZE)
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);

 out_DIO_StreamSetClocks:
    AIOUSB_UnLock();
    return result;
}

/*----------------------------------------------------------------------------*/
AIORESULT DIO_StreamFrame(
                          unsigned long DeviceIndex,
                          unsigned long FramePoints,
                          unsigned short *pFrameData,
                          unsigned long *BytesTransferred
                          ) 
{
    if( FramePoints == 0 || pFrameData == NULL || BytesTransferred == NULL )
        return AIOUSB_ERROR_INVALID_PARAMETER;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = NULL;
    USBDevice *deviceHandle = _check_dio_get_device_handle( DeviceIndex, &device, &result );

    if (!deviceHandle ) {
        return AIOUSB_ERROR_DEVICE_NOT_CONNECTED;
    } else if ( result != AIOUSB_SUCCESS ) {
        return result;
    }

    unsigned char endpoint = device->bDIORead ? (LIBUSB_ENDPOINT_IN | USB_BULK_READ_ENDPOINT) : (LIBUSB_ENDPOINT_OUT | USB_BULK_WRITE_ENDPOINT);
    int streamingBlockSize = ( int )device->StreamingBlockSize * sizeof(unsigned short);

    /**
     * @note convert parameter types to those that libusb likes
     */
    unsigned char *data = ( unsigned char* )pFrameData;
    int remaining = ( int )FramePoints * sizeof(unsigned short);
    int total = 0;
    while(remaining > 0) {
        int bytes;
        int libusbResult = deviceHandle->usb_bulk_transfer(deviceHandle, 
                                                           endpoint, 
                                                           data,
                                                           (remaining < streamingBlockSize) ? remaining : streamingBlockSize,
                                                           &bytes, 
                                                           device->commTimeout
                                                           );
        if(libusbResult == LIBUSB_SUCCESS) {
            if(bytes > 0) {
                total += bytes;
                data += bytes;
                remaining -= bytes;
            }
        } else {
            result = LIBUSB_RESULT_TO_AIOUSB_RESULT(libusbResult);
            break;
        }
    }
    if(result == AIOUSB_SUCCESS)
        *BytesTransferred = total;

    return result;
}


#ifdef __cplusplus
}
#endif

