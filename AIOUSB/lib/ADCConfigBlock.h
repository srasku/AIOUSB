#ifndef _ADC_CONFIG_BLOCK_H
#define _ADC_CONFIG_BLOCK_H

#include "AIOTypes.h"
#include "AIOEither.h"
#include <stdlib.h>
#include <string.h>

#ifdef __aiousb_cplusplus
namespace AIOUSB
{
#endif

typedef struct aio_usb_driver AIOUSBDevice ;

typedef struct mux_settings {
    unsigned long ADCChannelsPerGroup;
    unsigned long ADCMUXChannels;
    AIOUSB_BOOL defined;
} ADCMuxSettings;

typedef struct adc_config_block {
    AIOUSBDevice *device; /**< Pointer to the device Descriptor */
    unsigned long size;
    unsigned char registers[ AD_MAX_CONFIG_REGISTERS +1];
    unsigned timeout;
    ADCMuxSettings mux_settings;
    int clock_rate;
    AIOUSB_BOOL discardFirstSample;
    AIOUSB_BOOL debug;
    AIOUSB_BOOL testing; /**< For making Unit tests that don't talk to hardware */
} ADCConfigBlock;

typedef ADCConfigBlock ADConfigBlock;

typedef struct lookup { 
    int value;
    char *str;
    char *strvalue;
} EnumStringLookup;

#define STRINGIFY(x) #x


PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockInit( ADCConfigBlock *, AIOUSBDevice *deviceDesc, unsigned int );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockInitForCounterScan(ADCConfigBlock *config, AIOUSBDevice *deviceDesc  );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockInitializeDefault( ADCConfigBlock *config );
PUBLIC_EXTERN void ADC_VerifyAndCorrectConfigBlock( ADCConfigBlock *configBlock , AIOUSBDevice *deviceDesc  );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetAllGainCodeAndDiffMode(ADCConfigBlock *config, unsigned gainCode, AIOUSB_BOOL differentialMode);

PUBLIC_EXTERN unsigned char *ADCConfigBlockGetRegisters( ADCConfigBlock *config );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetRegister( ADCConfigBlock *config, unsigned reg, unsigned char value );

PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetGainCode( const  ADCConfigBlock *config, unsigned channel );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetGainCode(ADCConfigBlock *config, unsigned channel, unsigned char gainCode);

PUBLIC_EXTERN void ADCConfigBlockSetClockRate( ADCConfigBlock *config, int clock_rate);
PUBLIC_EXTERN int ADCConfigBlockGetClockRate( ADCConfigBlock *config );


PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetScanRange(ADCConfigBlock *config, unsigned startChannel, unsigned endChannel);
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetStartChannel( ADCConfigBlock *config, unsigned char startChannel  );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetEndChannel( ADCConfigBlock *config, unsigned char endChannel  );
#define HIGH_BITS(reg)   ( reg & 0xF0 )
#define LOW_BITS(reg)    ( reg & 0x0F )


PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetCalMode(ADCConfigBlock *config, ADCalMode calMode);
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetCalMode(const ADCConfigBlock *config);

PUBLIC_EXTERN char *ADCConfigBlockToYAML(ADCConfigBlock *config);


PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetStartChannel(const ADCConfigBlock *config);
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetEndChannel(const ADCConfigBlock *config);

PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetOversample( const ADCConfigBlock *config );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetOversample( ADCConfigBlock *config, unsigned overSample );

PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetTimeout( const ADCConfigBlock *config );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetTimeout( ADCConfigBlock *config, unsigned timeout );

PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetTriggerMode(const ADCConfigBlock *config);
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetTriggerMode(ADCConfigBlock *config, unsigned triggerMode);

PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetDifferentialMode(ADCConfigBlock *config, unsigned channel, AIOUSB_BOOL differentialMode);
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetRangeSingle(ADCConfigBlock *config, unsigned long channel, unsigned char gainCode);




PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockCopy( ADCConfigBlock *to, ADCConfigBlock *from );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetDevice( ADCConfigBlock *obj, AIOUSBDevice *dev );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetAIOUSBDevice( ADCConfigBlock *obj, AIOUSBDevice *dev );
PUBLIC_EXTERN AIOUSBDevice *ADCConfigBlockGetAIOUSBDevice( ADCConfigBlock *obj, AIORET_TYPE *res );
/* PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockInitialize( ADCConfigBlock *config , AIOUSBDevice *dev); */
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockInitializeFromAIOUSBDevice( ADCConfigBlock *config , AIOUSBDevice *dev);


PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetTesting( ADCConfigBlock *obj, AIOUSB_BOOL testing );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetTesting( const ADCConfigBlock *obj );

PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetSize( ADCConfigBlock *obj, unsigned size );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetSize( const ADCConfigBlock *obj );

PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockSetDebug( ADCConfigBlock *obj, AIOUSB_BOOL debug );
PUBLIC_EXTERN AIORET_TYPE ADCConfigBlockGetDebug( const ADCConfigBlock *obj );


/* JSON API */
char *ADCConfigBlockToJSON(ADCConfigBlock *config);
ADCConfigBlock *NewADCConfigBlockFromJSON( char *str );
AIORET_TYPE DeleteADCConfigBlock( ADCConfigBlock *config );


#ifndef SWIG
PUBLIC_EXTERN AIORET_TYPE _adccblock_valid_channel_settings(AIORET_TYPE in, ADCConfigBlock *config , int ADCMUXChannels );
PUBLIC_EXTERN AIORET_TYPE _adccblock_valid_size(AIORET_TYPE in, ADCConfigBlock *config );
PUBLIC_EXTERN AIORET_TYPE _adccblock_valid_cal_setting( AIORET_TYPE in, ADCConfigBlock *config );
PUBLIC_EXTERN AIORET_TYPE _adccblock_valid_end_channel( AIORET_TYPE in, ADCConfigBlock *config );
PUBLIC_EXTERN AIORET_TYPE _adccblock_valid_trigger_setting(ADCConfigBlock *config );
PUBLIC_EXTERN AIORET_TYPE _adcblock_valid_channel_settings(AIORET_TYPE in, ADCConfigBlock *config , int ADCMUXChannels );

#endif


#ifdef __aiousb_cplusplus
}
#endif


#endif
