/**
 * @file   AIOUSB_Core.c
 * @author $Format: %an <%ae>$
 * @date   $Format: %ad$
 * @version $Format: %t$
 * @brief  General header files for the AIOUSB library
 *
 */

#include "ADCConfigBlock.h"
#include "AIOUSB_Core.h"
#include "AIODeviceTable.h"
#include "aiousb.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <libusb.h>

#ifdef BACKTRACE
#include <execinfo.h>

#define BACKTRACE_DEBUG(n) do { \
          int j, nptrs; \
          void *buffer[100]; \
          char **strings; \
          nptrs = backtrace(buffer, 100); \
          nptrs = (nptrs > n ? n : nptrs); \
          strings = backtrace_symbols(buffer, nptrs); \
          for(j = 0; j < nptrs; j++) \
              printf("%s\n", strings[j]); \
  } while(0);
#else
#define BACKTRACE_DEBUG(n) {}
#endif


#ifdef __cplusplus
namespace AIOUSB {
#endif

int aio_errno;

static const ProductIDName productIDNameTable[] = {
    { USB_DA12_8A_REV_A , "USB-DA12-8A-A"  },
    { USB_DA12_8A       , "USB-DA12-8A"    },
    { USB_DA12_8E       , "USB-DA12-8E"    },
    { USB_DIO_32        , "USB-DIO-32"     },
    { USB_DIO_32I       , "USB-DIO-32"     },
    { USB_DIO_48        , "USB-DIO-48"     },
    { USB_DIO_96        , "USB-DIO-96"     },
    { USB_DI16A_REV_A1  , "USB-DI16A-A1"   },
    { USB_DO16A_REV_A1  , "USB-DO16A-A1"   },
    { USB_DI16A_REV_A2  , "USB-DI16A-A2"   },
    { USB_DIO_16H       , "USB-DIO-16H"    },
    { USB_DI16A         , "USB-DI16A"      },
    { USB_DO16A         , "USB-DO16A"      },
    { USB_DIO_16A       , "USB-DIO-16A"    },
    { USB_IIRO_16       , "USB-IIRO-16"    },
    { USB_II_16         , "USB-II-16"      },
    { USB_RO_16         , "USB-RO-16"      },
    { USB_IIRO_8        , "USB-IIRO-8"     },
    { USB_II_8          , "USB-II-8"       },
    { USB_IIRO_4        , "USB-IIRO-4"     },
    { USB_IDIO_16       , "USB-IDIO-16"    },
    { USB_II_16_OLD     , "USB-II-16-OLD"  },
    { USB_IDO_16        , "USB-IDO-16"     },
    { USB_IDIO_8        , "USB-IDIO-8"     },
    { USB_II_8_OLD      , "USB-II-8-OLD"   },
    { USB_IDIO_4        , "USB-IDIO-4"     },
    { USB_CTR_15        , "USB-CTR-15"     },
    { USB_IIRO4_2SM     , "USB-IIRO4-2SM"  },
    { USB_IIRO4_COM     , "USB-IIRO4-COM"  },
    { USB_DIO16RO8      , "USB-DIO16RO8"   },
    { PICO_DIO16RO8     , "PICO-DIO16RO8"  },
    { USB_AI16_16A      , "USB-AI16-16A"   },
    { USB_AI16_16E      , "USB-AI16-16E"   },
    { USB_AI12_16A      , "USB-AI12-16A"   },
    { USB_AI12_16       , "USB-AI12-16"    },
    { USB_AI12_16E      , "USB-AI12-16E"   },
    { USB_AI16_64MA     , "USB-AI16-64MA"  },
    { USB_AI16_64ME     , "USB-AI16-64ME"  },
    { USB_AI12_64MA     , "USB-AI12-64MA"  },
    { USB_AI12_64M      , "USB-AI12-64M"   },
    { USB_AI12_64ME     , "USB-AI12-64ME"  },
    { USB_AI16_32A      , "USB-AI16-32A"   },
    { USB_AI16_32E      , "USB-AI16-32E"   },
    { USB_AI12_32A      , "USB-AI12-32A"   },
    { USB_AI12_32       , "USB-AI12-32"    },
    { USB_AI12_32E      , "USB-AI12-32E"   },
    { USB_AI16_64A      , "USB-AI16-64A"   },
    { USB_AI16_64E      , "USB-AI16-64E"   },
    { USB_AI12_64A      , "USB-AI12-64A"   },
    { USB_AI12_64       , "USB-AI12-64"    },
    { USB_AI12_64E      , "USB-AI12-64E"   },
    { USB_AI16_96A      , "USB-AI16-96A"   },
    { USB_AI16_96E      , "USB-AI16-96E"   },
    { USB_AI12_96A      , "USB-AI12-96A"   },
    { USB_AI12_96       , "USB-AI12-96"    },
    { USB_AI12_96E      , "USB-AI12-96E"   },
    { USB_AI16_128A     , "USB-AI16-128A"  },
    { USB_AI16_128E     , "USB-AI16-128E"  },
    { USB_AI12_128A     , "USB-AI12-128A"  },
    { USB_AI12_128      , "USB-AI12-128"   },
    { USB_AI12_128E     , "USB-AI12-128E"  },
    { USB_AO16_16A      , "USB-AO16-16A"   },
    { USB_AO16_16       , "USB-AO16-16"    },
    { USB_AO16_12A      , "USB-AO16-12A"   },
    { USB_AO16_12       , "USB-AO16-12"    },
    { USB_AO16_8A       , "USB-AO16-8A"    },
    { USB_AO16_8        , "USB-AO16-8"     },
    { USB_AO16_4A       , "USB-AO16-4A"    },
    { USB_AO16_4        , "USB-AO16-4"     },
    { USB_AO12_16A      , "USB-AO12-16A"   },
    { USB_AO12_16       , "USB-AO12-16"    },
    { USB_AO12_12A      , "USB-AO12-12A"   },
    { USB_AO12_12       , "USB-AO12-12"    },
    { USB_AO12_8A       , "USB-AO12-8A"    },
    { USB_AO12_8        , "USB-AO12-8"     },
    { USB_AO12_4A       , "USB-AO12-4A"    },
    { USB_AO12_4        , "USB-AO12-4"     },
    { USB_AIO16_16A     , "USB-AIO16-16A"  },
    { USB_AIO16_16E     , "USB-AIO16-16E"  },
    { USB_AIO12_16A     , "USB-AIO12-16A"  },
    { USB_AIO12_16      , "USB-AIO12-16"   },
    { USB_AIO12_16E     , "USB-AIO12-16E"  },
    { USB_AIO16_64MA    , "USB-AIO16-64MA" },
    { USB_AIO16_64ME    , "USB-AIO16-64ME" },
    { USB_AIO12_64MA    , "USB-AIO12-64MA" },
    { USB_AIO12_64M     , "USB-AIO12-64M"  },
    { USB_AIO12_64ME    , "USB-AIO12-64ME" },
    { USB_AIO16_32A     , "USB-AIO16-32A"  },
    { USB_AIO16_32E     , "USB-AIO16-32E"  },
    { USB_AIO12_32A     , "USB-AIO12-32A"  },
    { USB_AIO12_32      , "USB-AIO12-32"   },
    { USB_AIO12_32E     , "USB-AIO12-32E"  },
    { USB_AIO16_64A     , "USB-AIO16-64A"  },
    { USB_AIO16_64E     , "USB-AIO16-64E"  },
    { USB_AIO12_64A     , "USB-AIO12-64A"  },
    { USB_AIO12_64      , "USB-AIO12-64"   },
    { USB_AIO12_64E     , "USB-AIO12-64E"  },
    { USB_AIO16_96A     , "USB-AIO16-96A"  },
    { USB_AIO16_96E     , "USB-AIO16-96E"  },
    { USB_AIO12_96A     , "USB-AIO12-96A"  },
    { USB_AIO12_96      , "USB-AIO12-96"   },
    { USB_AIO12_96E     , "USB-AIO12-96E"  },
    { USB_AIO16_128A    , "USB-AIO16-128A" },
    { USB_AIO16_128E    , "USB-AIO16-128E" },
    { USB_AIO12_128A    , "USB-AIO12-128A" },
    { USB_AIO12_128     , "USB-AIO12-128"  },
    { USB_AIO12_128E    , "USB-AIO12-128E" }
};
#ifdef __cplusplus
const int NUM_PROD_NAMES = sizeof(productIDNameTable) / sizeof(productIDNameTable[ 0 ]);
#else
#define NUM_PROD_NAMES (sizeof(productIDNameTable) / sizeof(productIDNameTable[ 0 ]))
#endif


#define AIOUSB_ENABLE_MUTEX

/* PRIVATE DeviceDescriptor deviceTable[ MAX_USB_DEVICES ]; */
#if defined(AIOUSB_ENABLE_MUTEX)
static pthread_mutex_t aiousbMutex;
#endif

/* unsigned long AIOUSB_INIT_PATTERN = 0x9b6773adul;       // random pattern */
/* unsigned long aiousbInit = 0;                   // == AIOUSB_INIT_PATTERN if AIOUSB module is initialized */

static const char VERSION_NUMBER[] = "$Format: %t$";
static const char VERSION_DATE[] = "$Format: %ad$";

AIORET_TYPE _check_cal_table( AIORET_TYPE in, const unsigned short calTable[] ) 
{
    if ( in  != AIOUSB_SUCCESS ) { 
        return in;
    } else if ( !calTable ) { 
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    } else {
        return AIOUSB_SUCCESS;
    }
}

static AIORET_TYPE _check_mutex( AIORET_TYPE in ) 
{
    if ( in  != AIOUSB_SUCCESS ) { 
       return in;
    } else if ( !AIOUSB_Lock() ) { 
        return -AIOUSB_ERROR_INVALID_MUTEX;
    } else {
        return AIOUSB_SUCCESS;
    }
}

static AIORET_TYPE _check_adcstream( AIORET_TYPE in, AIOUSBDevice *deviceDesc ) 
{
   if ( in  != AIOUSB_SUCCESS ) { 
      return in;
   } else if ( !deviceDesc ) { 
       return -AIOUSB_ERROR_INVALID_DEVICE_SETTING;
   } else if (deviceDesc->bADCStream == AIOUSB_FALSE ) {
       return -AIOUSB_ERROR_NOT_SUPPORTED;
   } else {
       return AIOUSB_SUCCESS;
   }
}

static AIORET_TYPE _check_query_cal( AIORET_TYPE in, unsigned long DeviceIndex ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    if ( in  != AIOUSB_SUCCESS ) { 
        return in;
    } else if ( (result = ADC_QueryCal(DeviceIndex)) != AIOUSB_SUCCESS ) {
        return -result;
    } else {
        return AIOUSB_SUCCESS;
    }
}


/**
 * @brief
 * Notes on mutual exclusion / threading:
 * - Our mutual exclusion scheme is _not_ intended to be bulletproof. It's primarily intended
 *   to ensure mutually exclusive access to deviceTable[] and other global variables. It does
 *   NOT ensure mutually exclusive access to the USB bus. In fact, we want to permit threads
 *   to communicate with multiple devices simultaneously, to the extent possible with USB.
 *
 * - Nor does this scheme prevent multiple threads from altering the configuration of the same
 *   device or communicating with the same device. In other words, it's entirely possible for
 *   one thread to configure and communicate with a device, only to have another thread come
 *   along and to the same. It's up to the users of this library to ensure that such a scenario
 *   doesn't occur.
 *
 * - This library does seek to permit one thread to control one device, and another thread to
 *   control another device. Each thread may then safely communicate with its own device and
 *   alter the portion of deviceTable[] that pertains to its device.
 *
 * - Our mutual exclusion scheme also permits two threads to cooperate in the operation of a
 *   single device, such as in cases where a background thread does the actual work and the
 *   foreground thread monitors the progress. In such a case, the background thread might update
 *   a status variable which the foreground thread monitors. This form of resource sharing is
 *   supported by our mutual exclusion scheme.
 */

AIOUSB_BOOL AIOUSB_Lock() 
{
    assert(AIOUSB_IsInit());
#if defined(AIOUSB_ENABLE_MUTEX)
    return(pthread_mutex_lock(&aiousbMutex) == 0);
#else
    return AIOUSB_TRUE;
#endif
}

AIOUSB_BOOL AIOUSB_UnLock() 
{
    assert(AIOUSB_IsInit());
#if defined(AIOUSB_ENABLE_MUTEX)
    return(pthread_mutex_unlock(&aiousbMutex) == 0);
#else
    return AIOUSB_TRUE;
#endif
}

/**
 * @details This function is intended to improve upon
 * libusb_bulk_transfer() by receiving or transmitting packets until
 * the entire transfer request has been satisfied; it intentionally
 * restarts the timeout each time a packet is received, so the timeout
 * parameter specifies the longest permitted delay between packets,
 * not the total time to complete the transfer request
 */
PRIVATE int AIOUSB_BulkTransfer(
                                struct libusb_device_handle *dev_handle,
                                unsigned char endpoint,
                                unsigned char *data,
                                int length,
                                int *transferred,
                                unsigned int timeout
                                ) 
{
    assert(dev_handle != 0 &&
           data != 0 &&
           transferred != 0);
    int libusbResult = LIBUSB_SUCCESS;
    int total = 0;
    while(length > 0) {
          int bytes;
          libusbResult = libusb_bulk_transfer(dev_handle, endpoint, data, length, &bytes, timeout);
          if(libusbResult == LIBUSB_SUCCESS) {
                if(bytes > 0) {
                      total += bytes;
                      data += bytes;
                      length -= bytes;
                  }
            } else if(libusbResult == LIBUSB_ERROR_TIMEOUT) {
            /**
             * @note
             * even if we get a timeout, some data may have been transferred; if so, then
             * this timeout is not an error; if we get a timeout and no data was transferred,
             * then treat it as an error condition
             */
                if(bytes > 0) {
                      total += bytes;
                      data += bytes;
                      length -= bytes;
                  } else
                    break;              // from while() and return timeout result
            } else
              break;            // from while() and return error result
      }
    *transferred = total;
    return libusbResult;
}

/*------------------------------------------------------------------------*/
/**
 * @todo Replace AIOUSB_Lock() with thread safe lock on a per device index basis
 * @todo Insert correct error messages into global error string in case of failure
 */
AIOUSBDevice *DeviceTableAtIndex_Lock( unsigned long DeviceIndex ) 
{ 
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return NULL;
    }
    if( deviceDesc ) 
        AIOUSB_Lock();
    return (AIOUSBDevice *)deviceDesc;
}

AIOUSBDevice *AIOUSB_GetDevice_Lock(unsigned long DeviceIndex, unsigned long *result) {

    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, result );
    if ( *result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return NULL;
    }
    if(!deviceDesc) {
        AIOUSB_UnLock();
        *result = AIOUSB_ERROR_DEVICE_NOT_FOUND;
    }
    return (AIOUSBDevice*)deviceDesc;
}

/**
 * @brief This function is deprecated.
 * @param DeviceIndex
 * @param BlockSize
 * @return 0 or greater if the blocksize is correct, negative number on
 *              error
 */
long AIOUSB_GetStreamingBlockSize(unsigned long DeviceIndex) 
{
    long BlockSize;

    if(!AIOUSB_Lock())
        return -AIOUSB_ERROR_INVALID_MUTEX;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return -result;
    }
    if(deviceDesc->bADCStream || deviceDesc->bDIOStream)
        BlockSize = deviceDesc->StreamingBlockSize;
    else
        BlockSize = -AIOUSB_ERROR_NOT_SUPPORTED;

    AIOUSB_UnLock();
    return BlockSize;
}

unsigned long AIOUSB_SetStreamingBlockSize(
                                           unsigned long DeviceIndex,
                                           unsigned long BlockSize
                                           ) 
{
     if( BlockSize == 0 || BlockSize > 31ul * 1024ul * 1024ul )
          return AIOUSB_ERROR_INVALID_PARAMETER;
     
     if(!AIOUSB_Lock())
          return AIOUSB_ERROR_INVALID_MUTEX;

     AIORESULT result = AIOUSB_SUCCESS;
     AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
     if ( result != AIOUSB_SUCCESS ){
         AIOUSB_UnLock();
         return result;
     }

     if (deviceDesc->bADCStream) {
          if((BlockSize & 0x1FF) != 0)
               BlockSize = (BlockSize & 0xFFFFFE00ul) + 0x200;
          deviceDesc->StreamingBlockSize = BlockSize;
     } else if (deviceDesc->bDIOStream) {
          if ((BlockSize & 0xFF) != 0)
               BlockSize = (BlockSize & 0xFFFFFF00ul) + 0x100;
          deviceDesc->StreamingBlockSize = BlockSize;
     } else
          result = AIOUSB_ERROR_NOT_SUPPORTED;

     AIOUSB_UnLock();
     return result;
}

unsigned long AIOUSB_ClearFIFO(
                                             unsigned long DeviceIndex,
                                             FIFO_Method Method
                                             ) {

    AIORESULT result = AIOUSB_SUCCESS;
    USBDevice *usb;
    int request;
    int bytesTransferred;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS )
        goto out_AIOUSB_ClearFIFO;

    if(!VALID_ENUM(FIFO_Method, Method)) {
        result =  AIOUSB_ERROR_INVALID_PARAMETER;

    }

    if(!AIOUSB_Lock()) {
        result =  AIOUSB_ERROR_INVALID_MUTEX;
        goto out_AIOUSB_ClearFIFO;
    }

    usb = AIOUSBDeviceGetUSBHandle( deviceDesc );

    if (!usb) {
        result = AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
        goto out_AIOUSB_ClearFIFO;
    }

    /*
     * translate method into vendor request message
     */
    switch(Method) {
        /* case CLEAR_FIFO_METHOD_IMMEDIATE: */
    default:
        request = AUR_GEN_CLEAR_FIFO;
        break;
        
    case CLEAR_FIFO_METHOD_AUTO:
        request = AUR_GEN_CLEAR_FIFO_NEXT;
        break;
        
    case CLEAR_FIFO_METHOD_IMMEDIATE_AND_ABORT:
        request = AUR_GEN_ABORT_AND_CLEAR;
        break;
        
    case CLEAR_FIFO_METHOD_WAIT:
        request = AUR_GEN_CLEAR_FIFO_WAIT;
        break;
    }

    AIOUSB_UnLock();
    bytesTransferred = usb->usb_control_transfer(usb,
                                                 USB_WRITE_TO_DEVICE, 
                                                 request, 
                                                 0, 
                                                 0, 
                                                 0, 
                                                     0 , 
                                                 deviceDesc->commTimeout
                                                 );
    if (bytesTransferred != 0) 
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);

 out_AIOUSB_ClearFIFO:
    AIOUSB_UnLock();
    return result;
}

/*----------------------------------------------------------------------------*/
const char *AIOUSB_GetVersion() 
{
    return VERSION_NUMBER;
}

/*----------------------------------------------------------------------------*/
const char *AIOUSB_GetVersionDate() 
{
    return VERSION_DATE;
}

double AIOUSB_GetMiscClock(
                           unsigned long DeviceIndex
                           ) 
{
    double clockHz = 0;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return clockHz;
    }

    if (AIOUSB_Lock()) {
        clockHz = deviceDesc->miscClockHz;
        AIOUSB_UnLock();
    }
    return clockHz;
}

AIORESULT AIOUSB_SetMiscClock(
                              unsigned long DeviceIndex,
                              double clockHz
                              ) 
{
    if(clockHz <= 0)
        return AIOUSB_ERROR_INVALID_PARAMETER;

    if(!AIOUSB_Lock())
        return AIOUSB_ERROR_INVALID_MUTEX;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }
    deviceDesc->miscClockHz = clockHz;

    AIOUSB_UnLock();
    return result;
}

AIORET_TYPE AIOUSB_GetCommTimeout(
                                  unsigned long DeviceIndex
                                  ) 
{
    AIORET_TYPE timeout = 1000;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return -result;
    }
    timeout = deviceDesc->commTimeout;

    return timeout;
}

AIORET_TYPE AIOUSB_SetCommTimeout(
                                    unsigned long DeviceIndex,
                                    unsigned timeout
                                    ) 
{
    if (!AIOUSB_Lock())
        return AIOUSB_ERROR_INVALID_MUTEX;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ) {
        AIOUSB_UnLock();
        return -result;
    }
    deviceDesc->commTimeout = timeout;

    AIOUSB_UnLock();
    return result;
}

/*----------------------------------------------------------------------------*/
unsigned long AIOUSB_Validate_Device(unsigned long DeviceIndex) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    if(!AIOUSB_Lock()) {
        result = AIOUSB_ERROR_INVALID_MUTEX;
        return result;
    }

    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    if(deviceDesc->bADCStream == AIOUSB_FALSE) {
          AIOUSB_UnLock();
          result = AIOUSB_ERROR_NOT_SUPPORTED;
          goto RETURN_AIOUSB_Validate_Device;
      }

    if((result = ADC_QueryCal(DeviceIndex)) != AIOUSB_SUCCESS) {
          AIOUSB_UnLock();
      }
RETURN_AIOUSB_Validate_Device:
    return result;
}

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param channel
 * @param counts
 * @return
 */
double AIOUSB_CountsToVolts(
                            unsigned long DeviceIndex,
                            unsigned channel,
                            unsigned short counts
                            )
{
    double volts;

    if(AIOUSB_ArrayCountsToVolts(DeviceIndex, channel, 1, &counts, &volts) != AIOUSB_SUCCESS)
        volts = 0.0;
    return volts;
}

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param startChannel
 * @param endChannel
 * @param counts
 * @return
 */
unsigned long AIOUSB_MultipleCountsToVolts(
                                           unsigned long DeviceIndex,
                                           unsigned startChannel,
                                           unsigned endChannel,
                                           const unsigned short counts[],     /* deviceDesc->ADCMUXChannels */
                                           double volts[]     /* deviceDesc->ADCMUXChannels */
                                           )
{
    return AIOUSB_ArrayCountsToVolts(DeviceIndex, 
                                     startChannel, 
                                     endChannel - startChannel + 1,
                                     counts + startChannel, 
                                     volts + startChannel
                                     );
}
/*------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param channel
 * @param volts
 * @return
 */
unsigned short AIOUSB_VoltsToCounts(
    unsigned long DeviceIndex,
    unsigned channel,
    double volts
    )
{
    unsigned short counts;

    if(AIOUSB_ArrayVoltsToCounts(DeviceIndex, channel, 1, &volts, &counts) != AIOUSB_SUCCESS)
        counts = 0;
    return counts;
}
/*------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param startChannel
 * @param endChannel
 * @param volts
 * @return
 */
unsigned long AIOUSB_MultipleVoltsToCounts(
    unsigned long DeviceIndex,
    unsigned startChannel,
    unsigned endChannel,
    const double volts[],     /* deviceDesc->ADCMUXChannels */
    unsigned short counts[]     /* deviceDesc->ADCMUXChannels */
    )
{
    return AIOUSB_ArrayVoltsToCounts(DeviceIndex, startChannel, endChannel - startChannel + 1,
                                     volts + startChannel, counts + startChannel);
}

/*------------------------------------------------------------------------*/
/**
 * @param config
 * @param DeviceIndex
 * @param defaults
 */
AIORET_TYPE AIOUSB_InitConfigBlock(ADCConfigBlock *config, unsigned long DeviceIndex, AIOUSB_BOOL defaults)
{
    assert(config);
    if ( !config ) 
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    /*
     * mark as uninitialized unless this function succeeds
     */
    config->device = 0;
    config->size = 0;
    if (!AIOUSB_Lock() )
        return -AIOUSB_ERROR_INVALID_MUTEX;

    
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return -result;
    }

    config->device   = deviceDesc;
    config->size     = deviceDesc->ConfigBytes;
    config->timeout  = deviceDesc->commTimeout;
    config->testing  = AIOUSB_FALSE;
    if ( config->size != AD_CONFIG_REGISTERS && config->size != AD_MUX_CONFIG_REGISTERS ) 
        return -AIOUSB_ERROR_INVALID_ADCCONFIG;

    if (defaults) {
        ADCConfigBlockSetAllGainCodeAndDiffMode(config, AD_GAIN_CODE_0_10V, AIOUSB_FALSE);
        ADCConfigBlockSetCalMode(config, AD_CAL_MODE_NORMAL);
        ADCConfigBlockSetTriggerMode(config, 0);
        ADCConfigBlockSetScanRange(config, 0, deviceDesc->ADCMUXChannels - 1);
        ADCConfigBlockSetOversample(config, 0);
    } else {
        memset( config->registers, 0, config->size );
    }
    AIOUSB_UnLock();
    return result;
}

/*------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param fileName
 * @return
 */
unsigned long AIOUSB_ADC_LoadCalTable(
                                      unsigned long DeviceIndex,
                                      const char *fileName
                                      )
{
    if(fileName == 0)
        return AIOUSB_ERROR_INVALID_PARAMETER;

    if(!AIOUSB_Lock())
        return AIOUSB_ERROR_INVALID_MUTEX;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    if(deviceDesc->bADCStream == AIOUSB_FALSE) {
          AIOUSB_UnLock();
          return AIOUSB_ERROR_NOT_SUPPORTED;
      }

    if((result = ADC_QueryCal(DeviceIndex)) != AIOUSB_SUCCESS) {
          AIOUSB_UnLock();
          return result;
      }

    AIOUSB_UnLock();
    unsigned short *const calTable = ( unsigned short* )malloc(CAL_TABLE_WORDS * sizeof(unsigned short));
    assert(calTable != 0);
    if(calTable != 0) {
          struct stat fileInfo;
          if(stat(fileName, &fileInfo) == 0) {
                if(fileInfo.st_size == CAL_TABLE_WORDS * sizeof(unsigned short)) {
                      FILE *const calFile = fopen(fileName, "r");
                      if(calFile != NULL) {
                            const size_t wordsRead = fread(calTable, sizeof(unsigned short), CAL_TABLE_WORDS, calFile);
                            fclose(calFile);
                            if(wordsRead == ( size_t )CAL_TABLE_WORDS)
                                result = AIOUSB_ADC_SetCalTable(DeviceIndex, calTable);
                            else
                                result = AIOUSB_ERROR_FILE_NOT_FOUND;
                        }else
                          result = AIOUSB_ERROR_FILE_NOT_FOUND;
                  }else
                    result = AIOUSB_ERROR_INVALID_DATA;              // file size incorrect
            }else
              result = AIOUSB_ERROR_FILE_NOT_FOUND;
          free(calTable);
      }else
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    return result;
}
/*------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param calTable
 * @return
 */
unsigned long AIOUSB_ADC_SetCalTable(
                                     unsigned long DeviceIndex,
                                     const unsigned short calTable[]
                                     )
{
    int SRAM_BLOCK_WORDS = 1024;       // can send 1024 words at a time to SRAM
    int sramAddress = 0, wordsRemaining = CAL_TABLE_WORDS;
    int wordsWritten, bytesTransferred , size_to_write, libusbResult;
    AIORESULT result = AIOUSB_SUCCESS;
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc =  AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    USBDevice *usb;
    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_mutex(_check_query_cal( _check_adcstream( _check_cal_table(retval, calTable ), deviceDesc ), DeviceIndex)),
                             usb,  
                             out_AIOUSB_ADC_SetCalTable 
                             );
 
   /**
     * @info send calibration table to SRAM one block at a time;
     * according to the documentation, the proper procedure is to bulk
     * transfer a block of calibration data to "endpoint 2" and then
     * send a control message to load it into the SRAM
     */
    
    AIOUSB_UnLock();

    while (wordsRemaining > 0) {
        wordsWritten = (wordsRemaining < SRAM_BLOCK_WORDS) ? wordsRemaining : SRAM_BLOCK_WORDS;
        size_to_write = wordsWritten * sizeof(unsigned short);
        libusbResult = usb->usb_bulk_transfer(usb,
                                              LIBUSB_ENDPOINT_OUT | USB_BULK_WRITE_ENDPOINT,
                                              ( unsigned char* )(calTable + sramAddress), 
                                              size_to_write,
                                              &bytesTransferred, 
                                              deviceDesc->commTimeout
                                              );

        if (libusbResult != LIBUSB_SUCCESS) {
            retval = LIBUSB_RESULT_TO_AIOUSB_RESULT(libusbResult);
            break;
        } else if (bytesTransferred != size_to_write ) {
            retval = AIOUSB_ERROR_INVALID_DATA;
            break;
        } else {
            bytesTransferred = usb->usb_control_transfer(usb,
                                                         USB_WRITE_TO_DEVICE, 
                                                         AUR_LOAD_BULK_CALIBRATION_BLOCK,
                                                         sramAddress, 
                                                         wordsWritten, 
                                                         0, 
                                                         0, 
                                                         deviceDesc->commTimeout
                                                         );
            if(bytesTransferred != 0) {
                retval = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
                break;
            }
        }
        wordsRemaining -= wordsWritten;
        sramAddress += wordsWritten;
    }

 out_AIOUSB_ADC_SetCalTable:
    AIOUSB_UnLock();
    return (AIORESULT)retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Performs a generic vendor USB write
 */
unsigned long GenericVendorWrite(
                                 unsigned long deviceIndex,
                                 unsigned char Request,
                                 unsigned short Value,
                                 unsigned short Index,
                                 void *bufData,
                                 unsigned long *bytes_written
                                 ) {
    /* unsigned long result; */
    /* AIOUSBDevice *deviceDesc = AIOUSB_GetDevice_Lock( deviceIndex, &result ); */
    AIORESULT result ;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( deviceIndex , &result );
    if ( result != AIOUSB_SUCCESS )
        return result;

    libusb_device_handle *deviceHandle;
    int bytesTransferred;

    if ( !deviceDesc || result != AIOUSB_SUCCESS )
        goto out_GenericVendorWrite;

    deviceHandle = AIOUSB_GetDeviceHandle(deviceIndex);

    if (!deviceHandle)  {
        result = AIOUSB_ERROR_DEVICE_NOT_CONNECTED;
        goto out_GenericVendorWrite;
    }

    AIOUSB_UnLock();  /*  unlock while communicating with device */
    bytesTransferred = libusb_control_transfer(deviceHandle,
                                               USB_WRITE_TO_DEVICE,
                                               Request,
                                               Value,
                                               Index,
                                               (unsigned char*)bufData,
                                               *bytes_written,
                                               deviceDesc->commTimeout
                                               );
    if (bytesTransferred != (int)*bytes_written) {
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
    }

out_GenericVendorWrite:
    AIOUSB_UnLock();
    return result;
}

/**
 * @brief Performs basic low level USB vendor request
 * @return
 */
AIORESULT GenericVendorRead(
                            unsigned long DeviceIndex,
                            unsigned char Request,
                            unsigned short Value,
                            unsigned short Index,
                            void *bufData,
                            unsigned long *bytes_read
                            ) 
{
    AIORESULT result = AIOUSB_SUCCESS;
    int bytesTransferred;

    if(!AIOUSB_Lock()) {
          result = AIOUSB_ERROR_INVALID_MUTEX;
          return result;
    }

    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    USBDevice *usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    if ( !usb ) {
        result = -AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
        goto RETURN_GenericVendorRead;
    }

    result = AIOUSB_EnsureOpen(DeviceIndex);
    if (result != AIOUSB_SUCCESS) {
        AIOUSB_UnLock();
        goto RETURN_GenericVendorRead;
    }

    AIOUSB_UnLock();
    bytesTransferred = usb->usb_control_transfer(usb,
                                                 USB_READ_FROM_DEVICE,
                                                 Request,
                                                 Value,
                                                 Index,
                                                 (unsigned char*)bufData,
                                                 *bytes_read,
                                                 deviceDesc->commTimeout
                                                 );
    if(bytesTransferred != (int)*bytes_read)
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);

RETURN_GenericVendorRead:
    AIOUSB_UnLock();
    return result;
}

#ifdef __cplusplus
}       /* namespace AIOUSB */
#endif

#ifdef SELF_TEST


#include <math.h>
#include "gtest/gtest.h"
#include "tap.h"
#include "AIOUSB_Core.h"
#include "AIODeviceTable.h"

#ifdef __cplusplus
using namespace AIOUSB;
#endif

TEST(AIOUSB_Core,FakeInit ) {

    AIOUSB_InitTest();    
    EXPECT_EQ(AIOUSB_TRUE, AIOUSB_IsInit() );

}

TEST(AIOUSB_Core,MockObjects) {
    int numDevices = 0;
    AIODeviceTableInit();    
    AIODeviceTableAddDeviceToDeviceTable( &numDevices, USB_AIO16_16A );
    EXPECT_EQ( numDevices, 1 );
    AIODeviceTableAddDeviceToDeviceTable( &numDevices, USB_DIO_32 );
    EXPECT_EQ( numDevices, 2 );

    EXPECT_EQ( ((DeviceDescriptor *)&deviceTable[0])->ProductID, USB_AIO16_16A  );
    EXPECT_EQ( ((DeviceDescriptor *)&deviceTable[1])->ProductID, USB_DIO_32  );

}


int main( int argc , char *argv[] ) 
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




