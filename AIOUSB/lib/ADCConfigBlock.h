#ifndef _ADC_CONFIG_BLOCK_H
#define _ADC_CONFIG_BLOCK_H

#include "AIOTypes.h"
#include <stdlib.h>
#include <string.h>

#ifdef __aiousb_cplusplus
namespace AIOUSB
{
#endif

typedef struct aio_usb_driver AIOUSBDevice ;


typedef struct adc_config_block {
    AIOUSBDevice *device; /**< Pointer to the device Descriptor */
    unsigned long size;
    AIOUSB_BOOL testing;          /**< For making Unit tests that don't talk to hardware */
    unsigned char registers[ AD_MAX_CONFIG_REGISTERS +1];
} ADCConfigBlock;

PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockCopyConfig( ADCConfigBlock *to, ADCConfigBlock *from );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetDevice( ADCConfigBlock *obj, AIOUSBDevice *dev );
PUBLIC_EXTERN AIOUSBDevice *ADCConfigBlockGetAIOUSBDevice( ADCConfigBlock *obj );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockInitialize( ADCConfigBlock *obj );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetTesting( ADCConfigBlock *obj, AIOUSB_BOOL testing );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetTesting( ADCConfigBlock *obj );


#ifdef __aiousb_cplusplus
}
#endif


#endif
