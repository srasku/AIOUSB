/**
 * @file   AIOContinuousBuffer.h
 * @author $Format: %an <%ae>$
 * @date   $Format: %ad$
 * @version $Format: %h$
 * @brief  
 *
 */

#ifndef _AIO_CONTINUOUS_BUFFER_H
#define _AIO_CONTINUOUS_BUFFER_H

#include "AIOTypes.h"
#include "AIOChannelMask.h"
#include "AIOUSB_ADC.h"
#include "AIOTypes.h"
#include "AIOFifo.h"
#include "AIOUSB_Core.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <libusb.h>
#include <string.h>
#include <math.h>


#ifdef __aiousb_cplusplus
namespace AIOUSB
{
#endif

typedef void *(*AIOUSB_WorkFn)( void *obj );

typedef struct aio_continuous_buf {
    void *(*callback)(void *object);
#ifdef HAS_PTHREAD
    pthread_t worker;
    pthread_mutex_t lock;
    pthread_attr_t tattr;
#endif
    AIOUSB_WorkFn work;
    int DeviceIndex;
    AIOFifoCounts *fifo;
    AIOBufferType *buffer;
    unsigned char *countsbuf;
    unsigned bufunitsize;
    unsigned hz;
    unsigned usbbuf_size;
    unsigned divisora;
    unsigned divisorb;
    unsigned basesize;
    unsigned size;
    unsigned num_oversamples;
    unsigned num_channels;
    unsigned num_scans;
    unsigned counter_control;
    unsigned timeout;
    AIORET_TYPE exitcode;
    AIOUSB_BOOL testing;
    AIOUSB_BOOL debug;
    unsigned extra;                     /**< Keeps track of under writes */
    AIOChannelMask *mask;               /**< Used for keeping track of channels */
    AIOBufferType *tmpbuf;
    unsigned tmpbufsize;
    volatile THREAD_STATUS status; /* Are we running, paused ..etc; */
    AIORET_TYPE (*PushN)( struct aio_continuous_buf *buf, unsigned short *frombuf, unsigned int N );
    AIORET_TYPE (*PopN)( struct aio_continuous_buf *buf, unsigned short *frombuf, unsigned int N );
} AIOContinuousBuf;

#define ROOTCLOCK 10000000

/*-----------------------------  Constructors  ------------------------------*/
PUBLIC_EXTERN AIOContinuousBuf *NewAIOContinuousBuf( unsigned long DeviceIndex , unsigned scancounts, unsigned number_channels );
PUBLIC_EXTERN AIOContinuousBuf *NewAIOContinuousBufWithoutConfig( unsigned long DeviceIndex, unsigned scancounts , unsigned num_channels , AIOUSB_BOOL counts );
PUBLIC_EXTERN AIOContinuousBuf *NewAIOContinuousBufForCounts( unsigned long DeviceIndex, unsigned scancounts, unsigned num_channels );
PUBLIC_EXTERN AIOContinuousBuf *NewAIOContinuousBufTesting( unsigned long DeviceIndex , unsigned scancounts , unsigned num_channels , AIOUSB_BOOL counts  );
PUBLIC_EXTERN AIOContinuousBuf *NewAIOContinuousBufForVolts( unsigned long DeviceIndex, unsigned scancounts, unsigned num_channels, unsigned num_oversamples );


PUBLIC_EXTERN AIOContinuousBuf *NewAIOContinuousBufRawSmart( unsigned long DeviceIndex, unsigned num_channels, unsigned num_scans,
                                                             unsigned unit_size, unsigned num_oversamples );

PUBLIC_EXTERN void DeleteAIOContinuousBuf( AIOContinuousBuf *buf );

/*-----------------------------  Replacements  ------------------------------*/


PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufInitConfiguration(  AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufInitADCConfigBlock( AIOContinuousBuf *buf, 
                                                              unsigned size, 
                                                              ADGainCode gainCode, 
                                                              AIOUSB_BOOL diffMode, 
                                                              unsigned char os, 
                                                              AIOUSB_BOOL dfs 
                                                              );

PUBLIC_EXTERN AIOUSB_WorkFn AIOContinuousBufGetCallback( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetCallback(AIOContinuousBuf *buf , void *(*work)(void *object ) );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetTesting( AIOContinuousBuf *buf, AIOUSB_BOOL testing );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetTesting( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSendPreConfig( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetStartAndEndChannel( AIOContinuousBuf *buf, unsigned startChannel, unsigned endChannel );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetAllGainCodeAndDiffMode( AIOContinuousBuf *buf, ADGainCode gain, AIOUSB_BOOL diff );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetDeviceIndex( AIOContinuousBuf *buf );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetOverSample( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetDiscardFirstSample(  AIOContinuousBuf *buf , AIOUSB_BOOL discard );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufNumberChannels( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetChannelMask( AIOContinuousBuf *buf, AIOChannelMask *mask );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufNumberSignals( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetChannelRange( AIOContinuousBuf *buf, unsigned startChannel, unsigned endChannel , unsigned gainCode );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSaveConfig( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetDeviceIndex( AIOContinuousBuf *buf , unsigned long DeviceIndex );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufNumberWriteScansInCounts(AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufResetDevice(AIOContinuousBuf *buf );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetOverSample( AIOContinuousBuf *buf, unsigned os );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetTimeout( AIOContinuousBuf *buf, unsigned timeout );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetTimeout( AIOContinuousBuf *buf );


PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetDebug( AIOContinuousBuf *buf, AIOUSB_BOOL debug );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetDebug( AIOContinuousBuf *buf );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetNumberScansToRead( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetNumberScansToRead( AIOContinuousBuf *buf , unsigned num_scans );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetRemainingWriteSize( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetUnitSize( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufReset( AIOContinuousBuf *buf );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufPushN(AIOContinuousBuf *buf ,unsigned short *frombuf, unsigned int N );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufPopN(AIOContinuousBuf *buf , unsigned short *frombuf, unsigned int N );


/*-----------------------------  Deprecated / Refactored   -------------------------------*/
#ifndef SWIG
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_InitConfiguration(  AIOContinuousBuf *buf ) ACCES_DEPRECATED("Please use AIOContinuousBufInitConfiguration");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SetCallback(AIOContinuousBuf *buf , void *(*work)(void *object ) ) ACCES_DEPRECATED("Please use AIOContinuousBufSetCallback");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SetTesting( AIOContinuousBuf *buf, AIOUSB_BOOL testing ) ACCES_DEPRECATED("Please use AIOContinuousBufSetTesting");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SendPreConfig( AIOContinuousBuf *buf ) ACCES_DEPRECATED("Please use AIOContinuousBufSendPreConfig");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SetAllGainCodeAndDiffMode( AIOContinuousBuf *buf, ADGainCode gain, AIOUSB_BOOL diff ) ACCES_DEPRECATED("Please use AIOContinuousBufSetAllGainCodeAndDiffMode");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SetStartAndEndChannel( AIOContinuousBuf *buf, unsigned startChannel, unsigned endChannel ) ACCES_DEPRECATED("Please use AIOContinuousBufSetStartAndEndChannel");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_GetDeviceIndex( AIOContinuousBuf *buf ) ACCES_DEPRECATED("Please use AIOContinuousBufGetDeviceIndex");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SetChannelRangeGain( AIOContinuousBuf *buf, unsigned startChannel, unsigned endChannel , unsigned gainCode ) ACCES_DEPRECATED("Please use AIOContinuousBufSetChannelRange");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_GetOverSample( AIOContinuousBuf *buf ) ACCES_DEPRECATED("Please use AIOContinuousBufGetOverSample");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SetOverSample( AIOContinuousBuf *buf, unsigned os ) ACCES_DEPRECATED("Please use AIOContinuousBufSetOverSample");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SetDiscardFirstSample(  AIOContinuousBuf *buf , AIOUSB_BOOL discard ) ACCES_DEPRECATED("Please use AIOContinuousBufSetDiscardFirstSample");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_NumberChannels( AIOContinuousBuf *buf ) ACCES_DEPRECATED("Please use AIOContinuousBufNumberChannels");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_NumberSignals( AIOContinuousBuf *buf ) ACCES_DEPRECATED("Please use AIOContinuousBufNumberSignals");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SaveConfig( AIOContinuousBuf *buf ) ACCES_DEPRECATED("Please use AIOContinuousBufSaveConfig");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SetChannelMask( AIOContinuousBuf *buf, AIOChannelMask *mask ) ACCES_DEPRECATED("Please use AIOContinuousBufSetChannelMask");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_SetDeviceIndex( AIOContinuousBuf *buf , unsigned long DeviceIndex ) ACCES_DEPRECATED("Please use AIOContinuousBufSetDeviceIndex");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_NumberWriteScansInCounts(AIOContinuousBuf *buf ) ACCES_DEPRECATED("Please use AIOContinuousBufNumberWriteScansInCounts");
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBuf_ResetDevice( AIOContinuousBuf *buf)  ACCES_DEPRECATED("Please use AIOContinuousBufResetDevice");
#endif
/*---------------------------  Done Deprecated  -----------------------------*/

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufLock( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufUnlock( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufCallbackStart( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetReadPosition( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetWritePosition( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufAvailableReadSize( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetSize( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetStatus( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetExitCode( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufReadIntegerScanCounts( AIOContinuousBuf *buf, unsigned short *tmp , unsigned tmpsize, unsigned size );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufReadCompleteScanCounts( AIOContinuousBuf *buf, unsigned short *read_buf, unsigned read_buf_size );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufReadIntegerNumberOfScans( AIOContinuousBuf *buf, unsigned short *read_buf, unsigned tmpbuffer_size, size_t num_scans );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufCountScansAvailable(AIOContinuousBuf *buf);

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetClock( AIOContinuousBuf *buf, unsigned int hz );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufEnd( AIOContinuousBuf *buf );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSimpleSetupConfig( AIOContinuousBuf *buf, ADGainCode gainCode );

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufRead( AIOContinuousBuf *buf, AIOBufferType *readbuf , unsigned readbufsize, unsigned size);
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufWrite( AIOContinuousBuf *buf, AIOBufferType *writebuf, unsigned wrbufsize, unsigned size, AIOContinuousBufMode flag );
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufWriteCounts( AIOContinuousBuf *buf, unsigned short *data, unsigned datasize, unsigned size , AIOContinuousBufMode flag );

PUBLIC_EXTERN AIORET_TYPE Launch( AIOUSB_WorkFn callback, AIOContinuousBuf *buf );

AIORET_TYPE AIOContinuousBufCleanup( AIOContinuousBuf *buf );



#ifdef __aiousb_cplusplus
}
#endif

#endif
