#ifndef _AIO_DEVICE_TABLE_H
#define _AIO_DEVICE_TABLE_H

#include "AIOUSB_Core.h"
#include "AIOTypes.h"
#include "libusb.h"
#include <stdlib.h>

#ifdef __aiousb_cplusplus
namespace AIOUSB
{
#endif


PUBLIC_EXTERN AIOUSBDevice *AIODeviceTableGetDeviceAtIndex( unsigned long index , AIORESULT *result );

#ifdef __aiousb_cplusplus
}
#endif

#endif
