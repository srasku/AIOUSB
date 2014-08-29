#ifndef _AIOUSB_ADC_H
#define _AIOUSB_ADC_H

#include "AIOTypes.h"
#include "ADCConfigBlock.h"
#include "USBDevice.h"

#ifdef __aiousb_cplusplus
namespace AIOUSB
{
#endif


AIORET_TYPE WriteConfigBlock( USBDevice *usb, ADCConfigBlock *config );
AIORET_TYPE ReadConfigBlock( USBDevice *usb, ADCConfigBlock *config );

PUBLIC_EXTERN AIORET_TYPE ADC_GetOversample( unsigned long DeviceIndex );
PUBLIC_EXTERN AIORET_TYPE ADC_SetOversample( unsigned long DeviceIndex, unsigned char Oversample );

PUBLIC_EXTERN AIORESULT ADC_InitConfigBlock( ADCConfigBlock *, void *deviceDesc, unsigned int );

PUBLIC_EXTERN void ADC_InitConfigBlockForTesting( ADCConfigBlock *, 
                                                  void *deviceDesc, 
                                                  unsigned int something, 
                                                  AIOUSB_BOOL foo);


PUBLIC_EXTERN void ADC_SetTestingMode( ADCConfigBlock *config, 
                                       AIOUSB_BOOL testing );

PUBLIC_EXTERN AIOUSB_BOOL ADC_GetTestingMode(ADCConfigBlock *config, 
                                             AIOUSB_BOOL testing );


PUBLIC_EXTERN AIORET_TYPE  ADC_GetChannelV(
                                            unsigned long DeviceIndex,
                                            unsigned long ChannelIndex,
                                            double *pBuf );
 
PUBLIC_EXTERN AIORET_TYPE ADC_GetScanV(
                                         unsigned long DeviceIndex,
                                         double *pBuf );
 
PUBLIC_EXTERN AIORET_TYPE ADC_SetAllGainCodeAndDiffMode( 
                                                        unsigned long DeviceIndex, 
                                                        unsigned gain, 
                                                        AIOUSB_BOOL differentialMode ) __attribute__ ((deprecated("Please use ADCConfigBlockSetAllGainCodeAndDiffMode instead")));

PUBLIC_EXTERN AIORET_TYPE ADC_GetScan(
                                         unsigned long DeviceIndex,
                                         unsigned short *pBuf );
 
PUBLIC_EXTERN AIORET_TYPE ADC_GetConfig(
                                          unsigned long DeviceIndex,
                                          unsigned char *pConfigBuf,
                                          unsigned long *ConfigBufSize );
 
PUBLIC_EXTERN AIORET_TYPE ADC_SetConfig(
                                          unsigned long DeviceIndex,
                                          unsigned char *pConfigBuf,
                                          unsigned long *ConfigBufSize );

PUBLIC_EXTERN AIORET_TYPE ADC_RangeAll(
                                         unsigned long DeviceIndex,
                                         unsigned char *pGainCodes,
                                         unsigned long bSingleEnded );

PUBLIC_EXTERN AIORET_TYPE ADC_Range1(
                                       unsigned long DeviceIndex,
                                       unsigned long ADChannel,
                                       unsigned char GainCode,
                                       unsigned long bSingleEnded );
 
PUBLIC_EXTERN AIORET_TYPE ADC_ADMode(
                                       unsigned long DeviceIndex,
                                       unsigned char TriggerMode,
                                       unsigned char CalMode 
                                       );
 
PUBLIC_EXTERN AIORET_TYPE ADC_SetScanLimits(
                                              unsigned long DeviceIndex,
                                              unsigned long StartChannel,
                                              unsigned long EndChannel );
 
PUBLIC_EXTERN AIORET_TYPE ADC_SetCal(
                                       unsigned long DeviceIndex,
                                       const char *CalFileName );
 
PUBLIC_EXTERN AIORET_TYPE ADC_QueryCal(
                                         unsigned long DeviceIndex );
 
PUBLIC_EXTERN AIORET_TYPE ADC_Initialize(
                                           unsigned long DeviceIndex,
                                           unsigned char *pConfigBuf,
                                           unsigned long *ConfigBufSize,
                                           const char *CalFileName );

PUBLIC_EXTERN AIORET_TYPE ADC_BulkAcquire(
                                            unsigned long DeviceIndex,
                                            unsigned long BufSize,
                                            void *pBuf );
 
PUBLIC_EXTERN AIORET_TYPE ADC_BulkPoll(
                                         unsigned long DeviceIndex,
                                         unsigned long *BytesLeft
                                         );

PUBLIC_EXTERN AIORET_TYPE  BulkAcquire(
                                       unsigned long DeviceIndex,
                                       AIOBuf *buf,
                                       int size
                                       );

PUBLIC_EXTERN AIORET_TYPE  BulkPoll(
                                    unsigned long DeviceIndex,
                                    AIOBuf *
                                    );


PUBLIC_EXTERN AIORET_TYPE AIOUSB_SetDiscardFirstSample(unsigned long DeviceIndex,AIOUSB_BOOL discard );


PUBLIC_EXTERN unsigned char *ADC_GetADCConfigBlock_Registers(
                                                             ADCConfigBlock *config
                                                             );



/* FastScan Functions */
PUBLIC_EXTERN AIORET_TYPE ADC_InitFastITScanV( unsigned long DeviceIndex );

PUBLIC_EXTERN AIORET_TYPE ADC_CreateFastITConfig(
                                                   unsigned long DeviceIndex,
                                                   int size
                                                   );

PUBLIC_EXTERN AIORET_TYPE ADC_ResetFastITScanV(unsigned long DeviceIndex);
 
PUBLIC_EXTERN AIORET_TYPE ADC_SetFastITScanVChannels(unsigned long DeviceIndex,unsigned long NewChannels);
 
PUBLIC_EXTERN AIORET_TYPE ADC_GetFastITScanV(unsigned long DeviceIndex,double *pData);

PUBLIC_EXTERN AIORET_TYPE ADC_GetITScanV(unsigned long DeviceIndex,double *pBuf);
 

PUBLIC_EXTERN AIORET_TYPE AIOUSB_InitConfigBlock( ADCConfigBlock *config, unsigned long DeviceIndex, AIOUSB_BOOL defaults );
PUBLIC_EXTERN AIOBuf *NewBuffer( unsigned int bufsize );
PUBLIC_EXTERN void DeleteBuffer( AIOBuf *buf );
PUBLIC_EXTERN AIOBuf *CreateSmartBuffer( unsigned long DeviceIndex );


PUBLIC_EXTERN AIORET_TYPE AIOUSB_ADC_LoadCalTable(unsigned long DeviceIndex,const char *fileName );
PUBLIC_EXTERN AIORET_TYPE AIOUSB_ADC_SetCalTable(unsigned long DeviceIndex,const unsigned short calTable[] );
PUBLIC_EXTERN AIORET_TYPE AIOUSB_ADC_InternalCal(unsigned long DeviceIndex,AIOUSB_BOOL autoCal,unsigned short returnCalTable[],const char *saveFileName );

PUBLIC_EXTERN void AIOUSB_SetRegister(ADCConfigBlock *cb,unsigned int Register,unsigned char value );
PUBLIC_EXTERN unsigned char AIOUSB_GetRegister( ADCConfigBlock *cb,unsigned int Register );
PUBLIC_EXTERN AIORET_TYPE AIOUSB_ADC_ExternalCal(unsigned long DeviceIndex,const double points[],int numPoints,unsigned short returnCalTable[],const char *saveFileName );

 
/*
 * ADCConfigBlock was created to act like a C++ "class" in order to facilitate configuring
 * the A/D subsystem and do so in a reliable manner; the functions below should be thought
 * of as class "methods" that operate on an "instance" of ADCConfigBlock; the method named
 * AIOUSB_InitConfigBlock() is akin to a class "constructor"; below is an example of how
 * to use these functions/methods
 *
 * ADCConfigBlock configBlock;
 * AIOUSB_InitConfigBlock( &configBlock, DeviceIndex, AIOUSB_TRUE );		 //call "constructor"
 *   ... set up other properties ...
 * ADC_SetConfig( DeviceIndex, configBlock.registers, &configBlock.size );	 //send configuration block to device
 */

#ifndef SWIG
PUBLIC_EXTERN void AIOUSB_SetAllGainCodeAndDiffMode( ADCConfigBlock *config, unsigned gainCode, AIOUSB_BOOL differentialMode )  __attribute__ ((deprecated("Please use ADCConfigBlockSetAllGainCodeAndDiffMode")));
PUBLIC_EXTERN unsigned AIOUSB_GetGainCode( const ADCConfigBlock *config, unsigned channel )  __attribute__ ((deprecated("Please use ADCConfigBlockGetGainCode")));
PUBLIC_EXTERN void AIOUSB_SetGainCode( ADCConfigBlock *config, unsigned channel, unsigned gainCode )  __attribute__ ((deprecated("Please use ADCConfigBlockSetGainCode")));
PUBLIC_EXTERN AIORET_TYPE AIOUSB_SetScanRange( ADCConfigBlock *config, unsigned startChannel, unsigned endChannel ) __attribute__ ((deprecated("Please use ADCConfigBlockSetScanRange")));


PUBLIC_EXTERN AIOUSB_BOOL AIOUSB_IsDifferentialMode( const ADCConfigBlock *config, unsigned channel )__attribute__ ((deprecated("Please use ADCConfigBlockIsDifferentialMode")));;
PUBLIC_EXTERN AIORET_TYPE AIOUSB_SetDifferentialMode( ADCConfigBlock *config, unsigned channel, AIOUSB_BOOL differentialMode ) __attribute__ ((deprecated("Please use ADCConfigBlockSetDifferentialMode")));
PUBLIC_EXTERN AIORET_TYPE AIOUSB_GetCalMode( ADCConfigBlock *config ) __attribute__ ((deprecated("Please use ADCConfigBlockGetCalMode")));;
PUBLIC_EXTERN AIORET_TYPE AIOUSB_SetCalMode( ADCConfigBlock *config, unsigned calMode ) __attribute__ ((deprecated("Please use ADCConfigBlockSetCalMode")));
PUBLIC_EXTERN AIORET_TYPE AIOUSB_GetTriggerMode( const ADCConfigBlock *config )__attribute__ ((deprecated("Please use ADCConfigBlockGetTriggerMode")));;
PUBLIC_EXTERN AIORET_TYPE AIOUSB_SetTriggerMode( ADCConfigBlock *config, unsigned triggerMode ) __attribute__ ((deprecated("Please use ADCConfigBlockSetTriggerMode")));;
PUBLIC_EXTERN AIORET_TYPE AIOUSB_GetStartChannel( const ADCConfigBlock *config ) __attribute__ ((deprecated("Please use ADCConfigBlockGetStartChannel")));
PUBLIC_EXTERN AIORET_TYPE AIOUSB_GetEndChannel( const ADCConfigBlock *config ) __attribute__ ((deprecated("Please use ADCConfigBlockGetEndChannel")));
PUBLIC_EXTERN AIORET_TYPE AIOUSB_GetOversample( const ADCConfigBlock *config ) __attribute__ ((deprecated("Please use ADCConfigBlockGetOversample")));
PUBLIC_EXTERN AIORET_TYPE AIOUSB_SetOversample( ADCConfigBlock *config, unsigned overSample ) __attribute__ ((deprecated("Please use ADCConfigBlockSetOversample")));
#endif
PUBLIC_EXTERN AIOUSBDevice *AIOUSB_GetDevice_NoCheck( unsigned long DeviceIndex  );


#ifdef __aiousb_cplusplus
}
#endif

#endif
