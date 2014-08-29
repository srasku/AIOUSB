/**
 * @file   AIOUSB_DAC.c
 * @author $Format: %an <%ae>$
 * @date   $Format: %ad$
 * @version $Format: %t$
 * @brief Core code to handle DACs on AIOUSB devices.
 */

#include "AIOUSB_DAC.h"
#include "AIOUSB_Core.h"
#include "AIODeviceTable.h"
#include "AIOUSBDevice.h"
#include <assert.h>
#include <math.h>
#include <string.h>

#ifdef __cplusplus
namespace AIOUSB {
#endif

static AIORET_TYPE _check_immdacs( AIORET_TYPE in, AIOUSBDevice *deviceDesc ) 
{ 
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( deviceDesc->ImmDACs == 0 ) {
        return -AIOUSB_ERROR_NOT_SUPPORTED;
    } else {
        return AIOUSB_SUCCESS;
    }
}

static AIORET_TYPE _check_dac_params ( AIORET_TYPE in, AIOUSBDevice *deviceDesc )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( deviceDesc->bDACStream && (deviceDesc->bDACOpen || deviceDesc->bDACClosing )) {
        return -AIOUSB_ERROR_OPEN_FAILED;
    } else {
        return AIOUSB_SUCCESS;
    }
}

static AIORET_TYPE _check_channel( AIORET_TYPE in, AIOUSBDevice *deviceDesc , int Channel )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( Channel >= (int)deviceDesc->ImmDACs ) {
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    } else {
        return AIOUSB_SUCCESS;
    }
}

AIORET_TYPE _check_dac_range( AIORET_TYPE in, 
                              AIOUSBDevice *deviceDesc , 
                              unsigned short *pDACData, 
                              unsigned long DACDataCount  
                              )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( pDACData == NULL || DACDataCount > 10000  ) {
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    } else {
        return AIOUSB_SUCCESS;
    }
}

AIORET_TYPE _check_dac_vrange( AIORET_TYPE in,
                               AIOUSBDevice *deviceDesc,
                               unsigned long RangeCode
                               )
{
   if ( in != AIOUSB_SUCCESS ) {
       return in;
   } else if ( RangeCode < DAC_RANGE_0_5V || RangeCode > DAC_RANGE_10V ) {
       return -AIOUSB_ERROR_INVALID_PARAMETER;
   } else {
       return AIOUSB_SUCCESS;
   }
}

AIORET_TYPE _check_board_range( AIORET_TYPE in, AIOUSBDevice *deviceDesc )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( deviceDesc->bDACBoardRange == AIOUSB_FALSE ) {
        return -AIOUSB_ERROR_NOT_SUPPORTED;
    } else {
        return AIOUSB_SUCCESS;
    }
}


/*----------------------------------------------------------------------------*/
unsigned long DACDirect(unsigned long DeviceIndex,unsigned short Channel,unsigned short Value)
{
  
    AIORESULT result = AIOUSB_SUCCESS;
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    USBDevice *usb = NULL;
    int bytesTransferred;
    
    EXIT_FN_IF_NO_VALID_USB( deviceDesc , retval, _check_mutex( _check_channel( _check_dac_params( _check_immdacs( retval, deviceDesc ),  deviceDesc ), deviceDesc, Channel )), usb, out_DACDirect  );


    AIOUSB_UnLock();
    bytesTransferred = usb->usb_control_transfer(usb,
                                                 USB_WRITE_TO_DEVICE, 
                                                 AUR_DAC_IMMEDIATE,
                                                 Value, 
                                                 Channel, 
                                                 0, 
                                                 0,
                                                 deviceDesc->commTimeout
                                                 );
    if (bytesTransferred != 0)
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);

 out_DACDirect:
    AIOUSB_UnLock();
    return result;
}

/* if (!AIOUSB_Lock()) */
/*     return AIOUSB_ERROR_INVALID_MUTEX; */
/* if ( result != AIOUSB_SUCCESS ) */
/*     goto out_DACDirect; */
/* if (deviceDesc->ImmDACs == 0) { */
/*     result = AIOUSB_ERROR_NOT_SUPPORTED; */
/*     goto out_DACDirect; */
/* } */
/* if ( deviceDesc->bDACStream && (deviceDesc->bDACOpen || deviceDesc->bDACClosing )) { */
/*     result = AIOUSB_ERROR_OPEN_FAILED; */
/*     goto out_DACDirect; */
/* } */
/* if (Channel >= deviceDesc->ImmDACs) { */
/*     result = AIOUSB_ERROR_INVALID_PARAMETER; */
/*     goto out_DACDirect; */
/* } */
/* usb = AIOUSBDeviceGetUSBHandle( deviceDesc ); */
/* if (!usb ) { */
/*     result = AIOUSB_ERROR_USBDEVICE_NOT_FOUND; */
/*     goto out_DACDirect; */
/* } */

/*----------------------------------------------------------------------------*/
/**
 * @brief pDACData is an array of DACDataCount channel/count 16-bit word pairs:
 * @verbatim
 *   +----------------+
 *   |    channel     | word 0
 *   |----------------|
 *   |     count      | word 1
 *   |----------------|
 *          ...
 *   |----------------|
 *   |    channel     |
 *   |----------------|
 *   |     count      | word ( DACDataCount * 2 ) - 1
 *   +----------------+
 *
 * this array has to be converted to a different format when passed to the board:
 *        Block 0
 *   +----------------+
 *   |   chan mask    | byte 0
 *   |----------------|
 *   |  chan 0 count  | bytes 1-2
 *   |----------------|
 *          ...
 *   |----------------|
 *   |  chan 6 count  | bytes 13-14
 *   |----------------|
 *   |  chan 7 count  | bytes 15-16
 *   +----------------+
 *        Block 1
 *   +----------------+
 *   |   chan mask    | byte 17
 *   |----------------|
 *   |  chan 0 count  | bytes 18-19
 *   |----------------|
 *          ...
 *   |----------------|
 *   |  chan 6 count  | bytes 30-31
 *   |----------------|
 *   |  chan 7 count  | bytes 32-33
 *   +----------------+
 *          ...
 *        Block n
 *   +----------------+
 *   |   chan mask    |
 *   |----------------|
 *   |  chan 0 count  |
 *   |----------------|
 *          ...
 *   |----------------|
 *   |  chan 6 count  |
 *   |----------------|
 *   |  chan 7 count  | bytes ( ( 17 * n ) - 2 ) - ( ( 17 * n ) - 1 )
 *   +----------------+
 *@endverbatim
 * the channel mask (the first byte of each block) has a bit set to
 * one for each channel whose output is to be set; the count values
 * are zero for channels that aren't to be set; for example, a mask of
 * 0x01 would write to only channel 0 on a given block; a mask of 0x80
 * would write to only channel 7
 *
 * since the DAC configuration blocks are contiguous, the byte offset
 * to a channel's count within the buffer containing all the
 * configuration blocks can be calculated as: offset = ( channel *
 * sizeof( unsigned short ) ) + ( channel / 8 ) + 1; although this
 * calculation is correct, it's difficult to follow, so the code below
 * uses a slightly less efficient calculation that's easier to
 * understand
 *
 * when sending the DAC configuration blocks to the device we have to
 * send all the blocks from block 0 up to the block containing the
 * highest channel number being set
 */

unsigned long DACMultiDirect( unsigned long DeviceIndex,
                              unsigned short *pDACData,
                              unsigned long DACDataCount
                              ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    int highestChannel = 0,channel,index, bytesTransferred;
    unsigned char *configBuffer = NULL;
    USBDevice *usb;
    int DACS_PER_BLOCK = 8, CONFIG_BLOCK_BYTES, numConfigBlocks, configBytes;

    EXIT_FN_IF_NO_VALID_USB( deviceDesc , retval,_check_mutex(_check_dac_params( _check_immdacs(retval,deviceDesc), deviceDesc )), usb, out_DACMultiDirect );
    /* EXIT_FN_IF_NO_VALID_USB( deviceDesc , retval, _check_mutex( _check_dac_params( _check_immdacs((AIORET_TYPE)result,deviceDesc), pDACData, DACDataCount)), usb,  out_DACMultiDirect ); */



/* if ( result != AIOUSB_SUCCESS ){ */
/*     AIOUSB_UnLock(); */
/*     return result; */
/* } */

/* if ( pDACData == NULL || DACDataCount > 10000 ) */
/*     return AIOUSB_ERROR_INVALID_PARAMETER; */

/* if (DACDataCount == 0) */
/*     return AIOUSB_SUCCESS; */

/* if (!AIOUSB_Lock()) { */
/*     result = AIOUSB_ERROR_INVALID_MUTEX; */
/* } */

/* if (deviceDesc->ImmDACs == 0) { */
/*     result = AIOUSB_ERROR_NOT_SUPPORTED; */
/*     goto out_ */
/* } */

/* if ( deviceDesc->bDACStream && (deviceDesc->bDACOpen || deviceDesc->bDACClosing) ) { */
/*     AIOUSB_UnLock(); */
/*     return AIOUSB_ERROR_OPEN_FAILED; */
/* } */

    /*
     * determine highest channel number addressed in pDACData; no checking is
     * performed to ensure that the same channel is not set more than once
     */

    for(index = 0; index < ( int )DACDataCount; index++) {
          channel = pDACData[ index * 2 ];            // channel/count pairs
          if (channel > highestChannel)
              highestChannel = channel;
      }

    if (highestChannel >= ( int )deviceDesc->ImmDACs) {
          AIOUSB_UnLock();
          return AIOUSB_ERROR_INVALID_PARAMETER;
      }

    AIOUSB_UnLock();                                                    // unlock while communicating with device
    DACS_PER_BLOCK = 8;
    CONFIG_BLOCK_BYTES = 1 /* mask */ + DACS_PER_BLOCK * sizeof(unsigned short) /* 16-bit counts */;
    numConfigBlocks = (highestChannel / DACS_PER_BLOCK) + 1;
    configBytes = CONFIG_BLOCK_BYTES * numConfigBlocks;

    configBuffer = ( unsigned char* )malloc(configBytes);

    if (!configBuffer ) {
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
        goto out_DACMultiDirect;
    }

    /**
     * sparsely populate DAC configuration blocks
     * zero out channel masks and count values for unused channels
     */
    memset(configBuffer, 0, configBytes);    

    for(index = 0; index < ( int )DACDataCount; index++) {
        channel = pDACData[ index * 2 ];        /* channel/count pairs */
        int maskOffset = (channel / DACS_PER_BLOCK) * CONFIG_BLOCK_BYTES;
        /*                first byte of block  +  skip mask byte  +   word within block    */
        int countOffset = maskOffset           +      1           +  (channel % DACS_PER_BLOCK) * sizeof(unsigned short);
        configBuffer[ maskOffset ] |= (1u << (channel % DACS_PER_BLOCK));
        *( unsigned short* )&configBuffer[ countOffset ] = pDACData[ index * 2 + 1 ];
    }

    bytesTransferred = usb->usb_control_transfer(usb,
                                                 USB_WRITE_TO_DEVICE, 
                                                 AUR_DAC_IMMEDIATE,
                                                 0, 
                                                 0, 
                                                 configBuffer, 
                                                 configBytes, 
                                                 deviceDesc->commTimeout
                                                 );
    if (bytesTransferred != configBytes)
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);

    free(configBuffer);
 out_DACMultiDirect:
    AIOUSB_UnLock();
    return retval;
}

/* if ( result != AIOUSB_SUCCESS ){ */
/*     AIOUSB_UnLock(); */
/*     return result; */
/* } */
/* if ( pDACData == NULL || DACDataCount > 10000 ) */
/*     return AIOUSB_ERROR_INVALID_PARAMETER; */
/* if (DACDataCount == 0) */
/*     return AIOUSB_SUCCESS; */
/* if (!AIOUSB_Lock()) { */
/*     result = AIOUSB_ERROR_INVALID_MUTEX; */
/* } */
/* if (deviceDesc->ImmDACs == 0) { */
/*     result = AIOUSB_ERROR_NOT_SUPPORTED; */
/*     goto out_ */
/* } */
/* if ( deviceDesc->bDACStream && (deviceDesc->bDACOpen || deviceDesc->bDACClosing) ) { */
/*     AIOUSB_UnLock(); */
/*     return AIOUSB_ERROR_OPEN_FAILED; */
/* } */

/*----------------------------------------------------------------------------*/
/*
 * @brief Sets the range code for the DAC
 * @param DeviceIndex
 * @param RangeCode
 * @return
 */
unsigned long DACSetBoardRange(unsigned long DeviceIndex,unsigned long RangeCode ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    USBDevice *usb = NULL;
    int bytesTransferred;
    EXIT_FN_IF_NO_VALID_USB( deviceDesc , retval, AIOUSB_SUCCESS, usb, out_DACSetBoardRange );

    AIOUSB_UnLock();
    bytesTransferred = usb->usb_control_transfer(usb, 
                                                 USB_WRITE_TO_DEVICE, 
                                                 AUR_DAC_RANGE,
                                                 RangeCode, 
                                                 0, 
                                                 0, 
                                                 0, 
                                                 deviceDesc->commTimeout
                                                 );
    if (bytesTransferred != 0)
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
    
 out_DACSetBoardRange:
    AIOUSB_UnLock();
    return result;
}



/*----------------------------------------------------------------------------*/
unsigned long DACOutputOpen(unsigned long DeviceIndex,double *pClockHz) 
{
  // TODO: this function is not yet implemented
    return AIOUSB_ERROR_NOT_SUPPORTED;
}


/*----------------------------------------------------------------------------*/
unsigned long DACOutputClose(unsigned long DeviceIndex,unsigned long bWait) 
{
  // TODO: this function is not yet implemented
    return AIOUSB_ERROR_NOT_SUPPORTED;
}


/*----------------------------------------------------------------------------*/
unsigned long DACOutputCloseNoEnd( unsigned long DeviceIndex, unsigned long bWait ) 
{
  // TODO: this function is not yet implemented
    return AIOUSB_ERROR_NOT_SUPPORTED;
}



/*----------------------------------------------------------------------------*/
unsigned long DACOutputSetCount(unsigned long DeviceIndex, unsigned long NewCount) 
{
  // TODO: this function is not yet implemented
    return AIOUSB_ERROR_NOT_SUPPORTED;
} 



/*----------------------------------------------------------------------------*/
unsigned long DACOutputFrame(unsigned long DeviceIndex,
                             unsigned long FramePoints,
                             unsigned short *FrameData
                             ) 
{
  // TODO: this function is not yet implemented
    return AIOUSB_ERROR_NOT_SUPPORTED;
}



/*----------------------------------------------------------------------------*/
unsigned long DACOutputFrameRaw(
                                unsigned long DeviceIndex,
                                unsigned long FramePoints,
                                unsigned short *FrameData
                                ) 
{
    // TODO: this function is not yet implemented
    return AIOUSB_ERROR_NOT_SUPPORTED;
}



/*----------------------------------------------------------------------------*/
unsigned long DACOutputStart(
                             unsigned long DeviceIndex
                             ) 
{
  // TODO: this function is not yet implemented
    return AIOUSB_ERROR_NOT_SUPPORTED;
}



/*----------------------------------------------------------------------------*/
unsigned long DACOutputSetInterlock(
                                    unsigned long DeviceIndex,
                                    unsigned long bInterlock
                                    ) {
  // TODO: this function is not yet implemented
    return AIOUSB_ERROR_NOT_SUPPORTED;
} 

#ifdef __cplusplus
}
#endif
