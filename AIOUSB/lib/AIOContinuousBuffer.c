/**
 * @file   AIOContinuousBuffer.c
 * @author $Format: %an <%ae>$
 * @date   $Format: %ad$
 * @version $Format: %h$
 * @brief This file contains the required structures for performing the continuous streaming
 *        buffers that talk to ACCES USB-AI* cards. The functionality in this file was wrapped
 *        up to provide a more unified interface for continuous streaming of acquisition data and 
 *        to provide the user with a simplified system of reads for actually getting the streaming
 *        data. The role of the continuous mode is to just create a thread in the background that
 *        handles the low level USB transactions for collecting samples. This thread will fill up 
 *        a data structure known as the AIOContinuousBuf that is implemented as a fifo.
 *        
 * @todo Make the number of channels in the ContinuousBuffer match the number of channels in the
 *       config object
 */

#include "AIOUSB_Log.h"
#include "AIOContinuousBuffer.h"
#include "ADCConfigBlock.h"
#include "AIOChannelMask.h"
#include "AIOUSB_Core.h"
#include "AIODeviceTable.h"
#include "AIOFifo.h"
#include "AIOCountsConverter.h"

#ifdef __cplusplus
namespace AIOUSB {
#endif

void *ConvertCountsToVoltsFunction( void *object );
void *RawCountsWorkFunction( void *object );

/*-----------------------------  Constructors  -----------------------------*/
AIOContinuousBuf *NewAIOContinuousBufForCounts( unsigned long DeviceIndex, unsigned scancounts, unsigned num_channels )
{
    assert( num_channels > 0 );
    AIOContinuousBuf *tmp = NewAIOContinuousBufRawSmart( DeviceIndex, num_channels, scancounts, sizeof(unsigned short),0);
    AIOContinuousBufSetCallback( tmp, RawCountsWorkFunction );
    tmp->PushN = AIOContinuousBufPushN;
    tmp->PopN  = AIOContinuousBufPopN;
    return tmp;
}

 AIOContinuousBuf *NewAIOContinuousBufForVolts( unsigned long DeviceIndex, unsigned scancounts, unsigned num_channels, unsigned num_oversamples )
{
    assert( num_channels > 0 );

    AIOContinuousBuf *tmp = NewAIOContinuousBufRawSmart( DeviceIndex, num_channels, scancounts, sizeof(double), num_oversamples ); 
    AIOContinuousBufSetCallback( tmp, ConvertCountsToVoltsFunction );
    tmp->PushN = AIOContinuousBufPushN;
    tmp->PopN  = AIOContinuousBufPopN;
    AIOFifoVoltsInitialize( (AIOFifoVolts*)tmp->fifo );
    return tmp;
}

AIOContinuousBuf *NewAIOContinuousBufRawSmart( unsigned long DeviceIndex, 
                                               unsigned num_channels,
                                               unsigned num_scans,
                                               unsigned unit_size,
                                               unsigned num_oversamples
                                               )
{
    assert( num_channels > 0 );
    AIOContinuousBuf *tmp  = (AIOContinuousBuf *)malloc(sizeof(AIOContinuousBuf));

    tmp->size             = num_channels * num_scans * (1+num_oversamples) * unit_size;
    tmp->buffer           = (AIOBufferType *)malloc( tmp->size*unit_size );
    tmp->bufunitsize      = unit_size;

    tmp->fifo             = NewAIOFifoCounts( num_channels * num_scans * unit_size / sizeof(uint16_t) );
    tmp->num_oversamples  = num_oversamples;
    tmp->mask             = NewAIOChannelMask( num_channels );

    if ( num_channels > 32 ) {
        char *bitstr = (char *)malloc( num_channels +1 );
        memset(bitstr, 49, num_channels ); /* Set all to 1s */
        bitstr[num_channels] = '\0';
        AIOChannelMaskSetMaskFromStr( tmp->mask, bitstr );
        free(bitstr);
    } else {
        AIOChannelMaskSetMaskFromInt( tmp->mask, (unsigned)-1 >> (BIT_LENGTH(unsigned)-num_channels) ); /**< Use all bits for each channel */
    }

    tmp->testing        = AIOUSB_FALSE;

    tmp->num_scans      = num_scans;
    tmp->num_channels   = num_channels;
    tmp->basesize       = unit_size;

    tmp->exitcode       = 0;
    tmp->usbbuf_size    = 128*512;

    tmp->DeviceIndex  = DeviceIndex;

    /* for acquisition */
    tmp->status       = NOT_STARTED;
    tmp->worker       = cont_thread;
    tmp->hz           = 100000; /**> Default value of 100khz  */
    tmp->timeout      = 1000;   /**> Default Timeout of 1000us  */
    tmp->extra        = 0;
    tmp->tmpbuf       = NULL;
    tmp->tmpbufsize   = 0;
#ifdef HAS_PTHREAD
    tmp->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;   /* Threading mutex Setup */
#endif
    AIOContinuousBufSetCallback( tmp , RawCountsWorkFunction );
   
    return tmp;
}
/**
 * @brief Constructor for AIOContinuousBuf object. Will set up the 
 * @param bufsize 
 * @param num_channels 
 * @return 
 * @todo Needs a smarter constructor for specifying the Initial mask .Currently won't work
 *       for num_channels > 32
 */
AIOContinuousBuf *NewAIOContinuousBufWithoutConfig( unsigned long DeviceIndex, 
                                                    unsigned scancounts , 
                                                    unsigned num_channels , 
                                                    AIOUSB_BOOL counts )
{
    assert( num_channels > 0 );
    AIOContinuousBuf *tmp  = (AIOContinuousBuf *)malloc(sizeof(AIOContinuousBuf));
    tmp->mask              = NewAIOChannelMask( num_channels );
    tmp->fifo              = NewAIOFifoCounts( num_channels * scancounts );
    if ( num_channels > 32 ) { 
        char *bitstr = (char *)malloc( num_channels +1 );
        memset(bitstr, 49, num_channels ); /* Set all to 1s */
        bitstr[num_channels] = '\0';
        AIOChannelMaskSetMaskFromStr( tmp->mask, bitstr );
        free(bitstr);
    } else {
        AIOChannelMaskSetMaskFromInt( tmp->mask, (unsigned)-1 >> (BIT_LENGTH(unsigned)-num_channels) ); /**< Use all bits for each channel */
    }
    tmp->testing      = AIOUSB_FALSE;
    tmp->size         = num_channels * scancounts;

    tmp->num_scans     = scancounts;
    tmp->num_channels = num_channels;

    if (  counts ) {
        tmp->buffer = (AIOBufferType *)malloc( tmp->size * sizeof(unsigned short));
        tmp->bufunitsize = sizeof(unsigned short);
    } else {
        tmp->buffer      = (AIOBufferType *)malloc( tmp->size *sizeof(AIOBufferType ));
        tmp->bufunitsize = sizeof(AIOBufferType);
    }
    tmp->basesize     = scancounts;
    tmp->exitcode     = 0;
    tmp->usbbuf_size  = 128*512;

    tmp->DeviceIndex  = DeviceIndex;

    tmp->status       = NOT_STARTED;
    tmp->worker       = cont_thread;
    tmp->hz           = 100000; /**> Default value of 100khz  */
    tmp->timeout      = 1000;   /**> Default Timeout of 1000us  */
    tmp->extra        = 0;
    tmp->tmpbuf       = NULL;
    tmp->tmpbufsize   = 0;
#ifdef HAS_PTHREAD
    tmp->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;   /* Threading mutex Setup */
#endif
    AIOContinuousBufSetCallback( tmp , ConvertCountsToVoltsFunction );

    return tmp;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_InitConfiguration(  AIOContinuousBuf *buf ) 
{
    return AIOContinuousBufInitConfiguration( buf );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufPushN(AIOContinuousBuf *buf ,unsigned short *frombuf, unsigned int N )
{
    return buf->fifo->PushN( buf->fifo, frombuf, N );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufPopN(AIOContinuousBuf *buf , unsigned short *frombuf, unsigned int N )
{
    return buf->fifo->PopN( buf->fifo, frombuf, N );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufInitADCConfigBlock( AIOContinuousBuf *buf, unsigned size, ADGainCode gainCode, AIOUSB_BOOL diffMode, unsigned char os, AIOUSB_BOOL dfs )
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIORET_TYPE retval = AIOUSB_SUCCESS;

    retval = AIOUSBDeviceSetTesting( AIODeviceTableGetDeviceAtIndex(  AIOContinuousBufGetDeviceIndex( buf ), &result ) , AIOContinuousBufGetTesting( buf ) );
    if ( retval != AIOUSB_SUCCESS )
        return retval;

    /* Set testing */
    retval = ADCConfigBlockSetTesting( 
                                      AIOUSBDeviceGetADCConfigBlock( AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ) , &result ) ),
                                      AIOContinuousBufGetTesting( buf )
                                       );
    if ( retval != AIOUSB_SUCCESS )
        return retval;

    /* Set the AIOUSBDevice */
    retval = ADCConfigBlockSetAIOUSBDevice( 
                                           AIOUSBDeviceGetADCConfigBlock( AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ) , &result ) ),
                                           AIODeviceTableGetDeviceAtIndex( 0, &result )
                                            );
    if ( retval != AIOUSB_SUCCESS )
        return retval;

    /* Set the size */
    retval = ADCConfigBlockSetSize( AIOUSBDeviceGetADCConfigBlock( AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ) , &result ) ),
                                    size 
                                    );
    if ( retval != AIOUSB_SUCCESS )
        return retval;

    AIOContinuousBufInitConfiguration( buf ); /* Needed to enforce Testing mode */
    AIOContinuousBufSetAllGainCodeAndDiffMode( buf, gainCode , diffMode );
    AIOContinuousBufSetOverSample( buf, os );
    AIOContinuousBufSetDiscardFirstSample( buf, dfs );
    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Sets up an AIOContinuousBuffer to perform Internal , counter based 
 *        scanning. 
 * 
 * @param buf Our AIOContinuousBuffer
 * @return AIOUSB_SUCCESS if successful,  value < 0 if not.
 *
 */
AIORET_TYPE AIOContinuousBufInitConfiguration(  AIOContinuousBuf *buf ) 
{
    ADCConfigBlock config = {0};
    unsigned long tmp;
    AIORET_TYPE retval = AIOUSB_SUCCESS;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf), &result );
    if ( result != AIOUSB_SUCCESS ) {
        AIOUSB_UnLock();
        return -result;
    }

    ADCConfigBlockInitForCounterScan( &config, deviceDesc );
    /* ADCConfigBlockInit( &config, deviceDesc, deviceDesc->ConfigBytes  ); */
    /* ADCConfigBlockCopy( &config, AIOUSBDeviceGetADCConfigBlock( deviceDesc ) ); */

    AIOContinuousBufSendPreConfig( buf );

    tmp = ADC_SetConfig( AIOContinuousBufGetDeviceIndex( buf ), config.registers, &config.size );
    if ( tmp != AIOUSB_SUCCESS ) {
        retval = -(AIORET_TYPE)tmp;
    }
        
    tmp = ADCConfigBlockCopy( AIOUSBDeviceGetADCConfigBlock( deviceDesc ), &config );
    if ( tmp != AIOUSB_SUCCESS ) {
        retval = -(AIORET_TYPE)tmp;
    }
    return retval;
}

/*----------------------------------------------------------------------------*/
AIOContinuousBuf *NewAIOContinuousBuf( unsigned long DeviceIndex , unsigned scancounts , unsigned num_channels )
{
    AIOContinuousBuf *tmp = NewAIOContinuousBufWithoutConfig( DeviceIndex,  scancounts, num_channels , AIOUSB_FALSE );
    return tmp;
}

/*----------------------------------------------------------------------------*/
AIOContinuousBuf *NewAIOContinuousBufTesting( unsigned long DeviceIndex , 
                                              unsigned scancounts , 
                                              unsigned num_channels , 
                                              AIOUSB_BOOL counts )
{
    AIOContinuousBuf *tmp = NewAIOContinuousBufWithoutConfig( DeviceIndex,  scancounts, num_channels , counts );
    tmp->testing = AIOUSB_TRUE;
    return tmp;
}

/*----------------------------------------------------------------------------*/
AIOBufferType *AIOContinuousBufCreateTmpBuf( AIOContinuousBuf *buf, unsigned size )
{
    if ( ! buf->tmpbuf || buf->tmpbufsize != size ) {
        if ( buf->tmpbuf )
            free(buf->tmpbuf);
        buf->tmpbuf = (AIOBufferType *)malloc(sizeof(AIOBufferType)*size);
        buf->tmpbufsize = size;
    }
    return buf->tmpbuf;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_SendPreConfig( AIOContinuousBuf *buf ) {
    return AIOContinuousBufSendPreConfig( buf );
}
AIORET_TYPE AIOContinuousBufSendPreConfig( AIOContinuousBuf *buf )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIORESULT result = AIOUSB_SUCCESS;
    unsigned wLength = 0x1, wIndex = 0x0, wValue = 0x0, bRequest = AUR_PROBE_CALFEATURE;
    int usbresult = 0;
    unsigned char data[1];
    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    if ( result != AIOUSB_SUCCESS )
        return -result;

    if (  !buf->testing ) {
        usbresult = usb->usb_control_transfer( usb,
                                               USB_READ_FROM_DEVICE,
                                               bRequest,
                                               wValue,
                                               wIndex,
                                               data,
                                               wLength,
                                               buf->timeout
                                               );
    }
    
    if (usbresult < 0 ) {
        retval = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT( usbresult );
    }
    return retval;
}

/*----------------------------------------------------------------------------*/
void AIOContinuousBuf_DeleteTmpBuf( AIOContinuousBuf *buf )
{
    if ( buf->tmpbuf || buf->tmpbufsize > 0 ) {
        free(buf->tmpbuf);
    }
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Destructor for AIOContinuousBuf object
 */
void DeleteAIOContinuousBuf( AIOContinuousBuf *buf )
{
    DeleteAIOChannelMask( buf->mask );
    AIOContinuousBuf_DeleteTmpBuf( buf );
    free( buf->buffer );
    DeleteAIOFifoCounts( buf->fifo );
    free( buf );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_SetCallback(AIOContinuousBuf *buf , void *(*work)(void *object ) ) { return AIOContinuousBufSetCallback( buf, work );}
AIORET_TYPE AIOContinuousBufSetCallback(AIOContinuousBuf *buf , void *(*work)(void *object ) )
{
    if (!buf )
        return -AIOUSB_ERROR_INVALID_AIOCONTINUOUS_BUFFER;
    AIOContinuousBufLock( buf );
    buf->callback = work;
    AIOContinuousBufUnlock( buf );
 
   return AIOUSB_SUCCESS;
}

static unsigned buffer_size( AIOContinuousBuf *buf )
{
    return buf->fifo->size;
}

void set_read_pos(AIOContinuousBuf *buf , unsigned pos )
{
    assert(buf);
    buf->fifo->read_pos = ( pos > buf->fifo->size-1 ? buf->fifo->size-1 : pos );
}

unsigned get_read_pos( AIOContinuousBuf *buf )
{
    return buf->fifo->read_pos;
}

void set_write_pos(AIOContinuousBuf *buf , unsigned pos )
{
    buf->fifo->write_pos = ( pos > buf->fifo->size-1 ? buf->fifo->size-1 : pos );
}

unsigned get_write_pos( AIOContinuousBuf *buf )
{
    return buf->fifo->write_pos;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufSetNumberScansToRead( AIOContinuousBuf *buf , unsigned num_scans )
{
    assert(buf);
    if (buf ) 
        buf->num_scans = num_scans;
    else
        return -AIOUSB_ERROR_INVALID_AIOCONTINUOUS_BUFFER;

    return  AIOUSB_SUCCESS;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufGetNumberScansToRead( AIOContinuousBuf *buf )
{
    return buf->num_scans;
}

/*----------------------------------------------------------------------------*/
unsigned AIOContinuousBuf_BufSizeForCounts( AIOContinuousBuf * buf) 
{
    return buffer_size(buf);
}

/*----------------------------------------------------------------------------*/
/**
 * @brief This is an internal function, don't use it externally as it 
 * will confuse you.
 *
 */
static unsigned write_size( AIOContinuousBuf *buf ) 
{
    unsigned retval = 0;
    unsigned read, write;
    read = (unsigned )get_read_pos(buf);
    write = (unsigned)get_write_pos(buf);
    if (  read > write ) {
        retval =  read - write;
    } else {
        return buffer_size(buf) - (get_write_pos (buf) - get_read_pos (buf));
    }
    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufGetRemainingWriteSize( AIOContinuousBuf *buf )
{
    return write_size(buf);
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufGetUnitSize( AIOContinuousBuf *buf )
{
    return buf->bufunitsize;
}

AIORET_TYPE AIOContinuousBufReset( AIOContinuousBuf *buf )
{
    AIOContinuousBufLock( buf );

    buf->fifo->Reset( (AIOFifo*)buf->fifo );

    AIOContinuousBufUnlock( buf );
    return AIOUSB_SUCCESS;
}

/*----------------------------------------------------------------------------*/
static unsigned write_size_num_scan_counts( AIOContinuousBuf *buf ) 
{
    float tmp = write_size(buf) / AIOContinuousBufNumberChannels(buf);
    if (  tmp > (int)tmp ) {
        tmp = (int)tmp;
    } else {
        tmp = ( tmp - 1 < 0 ? 0 : tmp -1 );
    }
    return (unsigned)tmp;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_NumberWriteScansInCounts( AIOContinuousBuf *buf ) { return AIOContinuousBufNumberWriteScansInCounts( buf ); }
AIORET_TYPE AIOContinuousBufNumberWriteScansInCounts( AIOContinuousBuf *buf ) 
{
    assert(buf);
    AIORET_TYPE num_channels = AIOContinuousBufNumberChannels(buf);
    if ( num_channels < AIOUSB_SUCCESS )
        return num_channels;
    return num_channels*write_size_num_scan_counts( buf ) ;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Returns the amount of data available in the buffer
 */
unsigned read_size( AIOContinuousBuf *buf ) 
{
    return ( buffer_size(buf) - write_size(buf) );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufGetReadPosition( AIOContinuousBuf *buf )
{
    assert(buf);
    /* return get_read_pos( buf ); */
    return buf->fifo->read_pos;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufGetWritePosition( AIOContinuousBuf *buf )
{
    assert(buf);
    /* return get_write_pos( buf ); */
    return buf->fifo->write_pos;
}

AIORET_TYPE AIOContinuousBufAvailableReadSize( AIOContinuousBuf *buf )
{
    assert(buf);
    return read_size(buf);
}

AIORET_TYPE AIOContinuousBufGetSize( AIOContinuousBuf *buf )
{
    assert(buf);
    return buf->fifo->delta( (AIOFifo*)buf->fifo );
}

AIORET_TYPE AIOContinuousBufGetStatus( AIOContinuousBuf *buf )
{
    assert(buf);
    return (AIORET_TYPE)buf->status;
}

AIORET_TYPE AIOContinuousBufGetExitCode( AIOContinuousBuf *buf )
{
    assert(buf);
    return buf->exitcode;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief returns the number of Scans accross all channels that still 
 *       remain in the buffer
 */
AIORET_TYPE AIOContinuousBufCountScansAvailable(AIOContinuousBuf *buf) 
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;

    retval = (AIORET_TYPE)buf->fifo->rdelta( (AIOFifo*)buf->fifo ) / ( buf->fifo->refsize * AIOContinuousBufNumberChannels(buf) );
    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief will read in an integer number of scan counts if there is room.
 * @param buf 
 * @param tmp 
 * @param size The size of the tmp buffer
 * @return 
 */
AIORET_TYPE AIOContinuousBufReadIntegerScanCounts( AIOContinuousBuf *buf, 
                                                   unsigned short *read_buf , 
                                                   unsigned tmpbuffer_size, 
                                                   unsigned size
                                                   )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    assert(buf);
    if ( !buf )
        return -AIOUSB_ERROR_INVALID_DEVICE_SETTING;

    if (  size < (unsigned)AIOContinuousBufNumberChannels(buf) ) {
        return -AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
    }

    int num_scans = AIOContinuousBufCountScansAvailable( buf );

    retval += buf->fifo->PopN( buf->fifo, read_buf, num_scans*AIOContinuousBufNumberChannels(buf) );
    retval /= AIOContinuousBufNumberChannels(buf);
    retval /= buf->fifo->refsize;

    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief will read in an integer number of scan counts if there is room.
 * @param buf 
 * @param tmp 
 * @param size The size of the tmp buffer
 * @return 
 */
AIORET_TYPE AIOContinuousBufReadIntegerNumberOfScans( AIOContinuousBuf *buf, 
                                                      unsigned short *read_buf , 
                                                      unsigned tmpbuffer_size, 
                                                      size_t num_scans
                                                      )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    int debug = 0;
    assert(buf);
    if ( !buf )
        return -AIOUSB_ERROR_INVALID_DEVICE_SETTING;

    if (  tmpbuffer_size < (unsigned)( AIOContinuousBufNumberChannels(buf)*num_scans ) ) {
        return -AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
    }

    for ( int i = 0, pos = 0;  i < (int)num_scans && (int)( pos + AIOContinuousBufNumberChannels(buf)-1 ) < (int)tmpbuffer_size ; i++ , pos += AIOContinuousBufNumberChannels(buf) ) {
        if (  i == 0 )
            retval = AIOUSB_SUCCESS;
        if (  debug ) { 
            printf("Using i=%d\n",i );
        }
        retval += AIOContinuousBufRead( buf, (AIOBufferType *)&read_buf[pos] , tmpbuffer_size - pos, AIOContinuousBufNumberChannels(buf) );
        retval /= AIOContinuousBufNumberChannels(buf);
    }

    return retval;
}


/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufReadCompleteScanCounts( AIOContinuousBuf *buf, 
                                                    unsigned short *read_buf, 
                                                    unsigned read_buf_size
                                                    )
{

    AIORET_TYPE retval = AIOContinuousBufReadIntegerScanCounts( buf, 
                                                                read_buf, 
                                                                read_buf_size, 
                                                                MIN((int)read_buf_size, (int)AIOContinuousBufCountScansAvailable(buf)*AIOContinuousBufNumberChannels(buf) )
                                                                );
    return retval;
}



/*----------------------------------------------------------------------------*/
/**
 * @brief Returns 
 * @param buf 
 * @return Pointer to our work function
 */
AIOUSB_WorkFn AIOContinuousBufGetCallback( AIOContinuousBuf *buf )
{
    return buf->callback;
}

AIORET_TYPE AIOContinuousBufSetClock( AIOContinuousBuf *buf, unsigned int hz )
{
    assert(buf);
    buf->hz = MIN( (unsigned)hz, (unsigned)(ROOTCLOCK / ( (AIOContinuousBufGetOverSample(buf)+1) * AIOContinuousBufNumberChannels(buf))) );

    return AIOUSB_SUCCESS;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Starts the work function
 * @param buf 
 * @param work 
 * @return status code of start.
 */
AIORET_TYPE AIOContinuousBufStart( AIOContinuousBuf *buf )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
#ifdef HAS_PTHREAD
    buf->status = RUNNING;
#ifdef HIGH_PRIORITY            /* Must run as root if you use this */
    int fifo_max_prio;
    struct sched_param fifo_param;
    pthread_attr_t custom_sched_attr;
    pthread_attr_init( &custom_sched_attr ) ;
    pthread_attr_setinheritsched(&custom_sched_attr, PTHREAD_EXPLICIT_SCHED);
    pthread_attr_setschedpolicy(&custom_sched_attr, SCHED_RR);
    fifo_max_prio = sched_get_priority_max(SCHED_RR);
    fifo_param.sched_priority = fifo_max_prio;
    pthread_attr_setschedparam( &custom_sched_attr, &fifo_param);
    retval = pthread_create( &(buf->worker), &custom_sched_attr, buf->callback, (void *)buf );
#else
    retval = pthread_create( &(buf->worker), NULL, buf->callback, (void *)buf );
#endif
    if (  retval != 0 ) {
        buf->status = TERMINATED;
        AIOUSB_ERROR("Unable to create thread for Continuous acquisition");
        return -1;
    }
#endif  

    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Calculates the register values for buf->divisora, and buf->divisorb to create
 * an output clock that matches the value stored in buf->hz
 * @param buf AIOContinuousBuf object that we will be reading data into
 * @return Success(0) or failure( < 0 ) if we can't set the clocks
 */
AIORET_TYPE CalculateClocks( AIOContinuousBuf *buf )
{
    assert(buf);
    int  hz = (int)buf->hz;
    float l;

    int divisora, divisorb, divisorab;
    int min_err, err;

    if (  hz == 0 ) {
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    }
    if (   hz * 4 >= ROOTCLOCK ) {
        divisora = 2;
        divisorb = 2;
    } else { 
        divisorab = ROOTCLOCK / hz;
        l = sqrt( divisorab );
        if ( l > 0xffff ) { 
            divisora = 0xffff;
            divisorb = 0xffff;
            min_err  = abs((int)(round(((ROOTCLOCK / hz) - (int)(divisora * l)))));
        } else  { 
            divisora  = round( divisorab / l );
            l         = round(sqrt( divisorab ));
            divisorb  = l;

            min_err = abs(((ROOTCLOCK / hz) - (int)(divisora * l)));
      
            for( unsigned lv = l ; lv >= 2 ; lv -- ) {
                unsigned olddivisora = (int)round((double)divisorab / lv);
                if (  olddivisora > 0xffff ) { 
                    AIOUSB_DEVEL( "Found value > 0xff..resetting" );
                    break;
                } else { 
                    divisora = olddivisora;
                }

                err = abs((int)((ROOTCLOCK / hz) - (divisora * lv)));
                if (  err <= 0  ) {
                    min_err = 0;
                    AIOUSB_DEVEL("Found zero error: %d\n", lv );
                    divisorb = lv;
                    break;
                } 
                if (  err < min_err  ) {
                    AIOUSB_DEVEL( "Found new error: using lv=%d\n", (int)lv);
                    divisorb = lv;
                    min_err = err;
                }
                divisora = (int)round(divisorab / divisorb);
            }
        }
    }
    buf->divisora = divisora;
    buf->divisorb = divisorb;
    return AIOUSB_SUCCESS;
}

/** create thread to launch function */
AIORET_TYPE Launch( AIOUSB_WorkFn callback, AIOContinuousBuf *buf )
{
    assert(buf);
    AIORET_TYPE retval = pthread_create( &(buf->worker), NULL , callback, (void *)buf  );
    if (  retval != 0 ) {
        retval = -abs(retval);
    }
    return retval;
}

/**
 * @brief Sets the channel mask
 * @param buf 
 * @param mask 
 * @return 
 */
AIORET_TYPE AIOContinuousBuf_SetChannelMask( AIOContinuousBuf *buf, AIOChannelMask *mask ) { return AIOContinuousBufSetChannelMask( buf, mask ); }
AIORET_TYPE AIOContinuousBufSetChannelMask( AIOContinuousBuf *buf, AIOChannelMask *mask )
{
    assert(buf);
    assert(mask);
    buf->mask   = mask;
    buf->extra  = 0;
    return 0;
}

AIORET_TYPE AIOContinuousBuf_NumberSignals( AIOContinuousBuf *buf ) { return AIOContinuousBufNumberSignals( buf ); }
AIORET_TYPE AIOContinuousBufNumberSignals( AIOContinuousBuf *buf )
{
    assert(buf);
    return AIOChannelMaskNumberSignals(buf->mask );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_NumberChannels( AIOContinuousBuf *buf ) { return AIOContinuousBufNumberChannels(buf); }
AIORET_TYPE AIOContinuousBufNumberChannels( AIOContinuousBuf *buf ) 
{
    assert(buf);
    return AIOChannelMaskNumberSignals(buf->mask );
}

/*----------------------------------------------------------------------------*/
/**
 * @brief A simple copy of one ushort buffer to one of AIOBufferType and converts
 *       counts to voltages
 * @param buf 
 * @param channel 
 * @param data 
 * @param count 
 * @param tobuf 
 * @param pos 
 * @return retval the number of data elements that were written to the tobuf
 */
AIORET_TYPE AIOContinuousBuf_SmartCountsToVolts( AIOContinuousBuf *buf,  
                                                 unsigned *channel,
                                                 unsigned short *data, 
                                                 unsigned count,  
                                                 AIOBufferType *tobuf, 
                                                 unsigned *pos )
{
    assert(buf);
    AIORET_TYPE retval = 0;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf), &result );
    if ( result != AIOUSB_SUCCESS ) {
        AIOUSB_UnLock();
        return -result;
    }

    int number_channels = AIOContinuousBufNumberChannels(buf);
    assert(channel);
    if (  ! deviceDesc ) {
        retval = -1;
    } else {
      for(unsigned ch = 0; ch < count;  ch ++ , *channel = ((*channel+1)% number_channels ) , *pos += 1 ) {
          unsigned gain = ADC_GainCode_Cached( &deviceDesc->cachedConfigBlock , *channel );
          struct ADRange *range = &adRanges[ gain ];
          tobuf[ *pos ] = ( (( double )data[ ch ] / ( double )AI_16_MAX_COUNTS) * range->range ) + range->minVolts;
          retval += 1;
      }
   }
    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Allows one to write into the AIOContinuousBuf buffer a given amount (size) of data.
 * @param buf 
 * @param writebuf 
 * @param size 
 * @param flag
 * @return Status of whether the write was successful , if so returning the number of bytes written
 *         or if there was insufficient space, it returns negative error code. If the number 
 *         is >= 0, then this corresponds to the number of bytes that were written into the buffer.
 */
AIORET_TYPE AIOContinuousBufWrite( AIOContinuousBuf *buf, 
                                   AIOBufferType *writebuf, 
                                   unsigned wrbufsize, 
                                   unsigned size, 
                                   AIOContinuousBufMode flag )
{
    AIORET_TYPE retval;
    ERR_UNLESS_VALID_ENUM( AIOContinuousBufMode ,  flag );
    
    /* First try to lock the buffer */
    /* printf("trying to lock buffer for write\n"); */
    AIOContinuousBufLock( buf );
    int N = size / (buf->fifo->refsize );

    retval = buf->fifo->PushN( buf->fifo, writebuf, N );
    retval = ( retval == 0 ? -AIOUSB_ERROR_NOT_ENOUGH_MEMORY : retval );

    AIOContinuousBufUnlock( buf );
    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufWriteCounts( AIOContinuousBuf *buf, unsigned short *data, unsigned datasize, unsigned size , AIOContinuousBufMode flag )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    /* retval += AIOContinuousBufWrite( buf, (AIOBufferType *)data, datasize, size , flag  ); */
    retval += buf->fifo->PushN( buf->fifo, data, size / sizeof(unsigned short));

    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE aiocontbuf_get_data( AIOContinuousBuf *buf, 
                                 USBDevice *usb, 
                                 unsigned char endpoint, 
                                 unsigned char *data,
                                 int datasize,
                                 int *bytes,
                                 unsigned timeout 
                                 )
{
    AIORET_TYPE usbresult;

    usbresult = usb->usb_bulk_transfer( usb,
                                        0x86,
                                        data,
                                        datasize,
                                        bytes,
                                        timeout
                                        );

    return usbresult;
}


/*----------------------------------------------------------------------------*/
void *RawCountsWorkFunction( void *object )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIORESULT result = AIOUSB_SUCCESS;
    int usbresult;
    AIOContinuousBuf *buf = (AIOContinuousBuf*)object;
    int bytes;
    srand(3);
    /* unsigned datasize = AIOContinuousBufNumberChannels(buf)*16*512; */
    unsigned datasize = 64*1024;

    int usbfail = 0;
    int usbfail_count = 5;
    unsigned char *data  = (unsigned char *)malloc( datasize );
    unsigned count = 0;
    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );

    if ( result != AIOUSB_SUCCESS ) {
        buf->exitcode = -(AIORET_TYPE)result;
        retval = -result;
        goto out_RawCountsWorkFunction;
    }

    while ( buf->status == RUNNING  ) {

#ifdef TESTING
        FILE *tmpf;
        tmpf = fopen("tmpdata.txt","w");
        unsigned short *usdata = (unsigned short *)&data[0];
        int tval = MIN(AIOContinuousBuf_NumberWriteScansInCounts(buf)/AIOContinuousBufNumberChannels(buf), 
                       datasize / 2 / AIOContinuousBufNumberChannels(buf) );
        int trand = (rand() % tval + 1 );
        bytes = 2*AIOContinuousBufNumberChannels(buf)*trand;
        for( int i = 0, ch = 0; i < AIOContinuousBufNumberChannels(buf)*trand; i ++ , ch = ((ch+1)%AIOContinuousBufNumberChannels(buf))) {
            usdata[i] = ch*1000 + rand()%20;
            fprintf(tmpf, "%u,",usdata[i] );
            if (  (ch +1) % AIOContinuousBufNumberChannels(buf) == 0 ) {
                totalcount ++;
                fprintf(tmpf,"\n",usdata[i] );
            }
        }
        printf("");
#else

        usbresult = aiocontbuf_get_data( buf, usb, 0x86, data, datasize, &bytes, 3000 );

#endif

        AIOUSB_DEVEL("libusb_bulk_transfer returned  %d as usbresult, bytes=%d\n", usbresult , (int)bytes);

        if (  bytes ) {
            /* only write bytes that exist */
            bytes = ( AIOContinuousBuf_BufSizeForCounts(buf) - buf->fifo->refsize - count < datasize ? AIOContinuousBuf_BufSizeForCounts(buf) - buf->fifo->refsize - count : bytes );

            int tmp = buf->fifo->PushN( buf->fifo, (uint16_t*)data, bytes / sizeof(unsigned short));

            AIOUSB_DEVEL("Pushed %d, size: %d\n", bytes / 2 , buf->fifo->size );

            if (  tmp >= 0 ) {
                count += tmp;
            }

            AIOUSB_DEVEL("Tmpcount=%d,count=%d,Bytes=%d, Write=%d,Read=%d,max=%d\n", tmp,count,bytes,get_write_pos(buf) , get_read_pos(buf),buffer_size(buf));

            /**
             * Modification, allow the count to keep going... stop 
             * if 
             * 1. count >= number we are supposed to read
             * 2. we don't have enough space
             */
            if (  count >= AIOContinuousBuf_BufSizeForCounts(buf) - AIOContinuousBufNumberChannels(buf) ) {
            /* if ( count >= AIOContinuousBufGetNumberScansToRead(buf) - AIOContinuousBufNumberChannels(buf) ) {  */
            /* if ( count > buf->num_scans*buf->num_channels ) {  */
                AIOContinuousBufLock(buf);
                buf->status = TERMINATED;
                AIOContinuousBufUnlock(buf);
            }
        } else if (  usbresult < 0  && usbfail < usbfail_count ) {
            AIOUSB_ERROR("Error with usb: %d\n", (int)usbresult );
            usbfail ++;
        } else {
            if (  usbfail >= usbfail_count  ){
                AIOUSB_ERROR("Erroring out. too many usb failures: %d\n", usbfail_count );
                retval = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT(usbresult);
                AIOContinuousBufLock(buf);
                buf->status = TERMINATED;
                AIOContinuousBufUnlock(buf);
                buf->exitcode = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT(usbresult);
            } 
        }
    }
#ifdef TESTING
    fclose(tmpf);
#endif
 out_RawCountsWorkFunction:
    AIOContinuousBufLock(buf);
    buf->status = TERMINATED;
    AIOContinuousBufUnlock(buf);
    AIOUSB_DEVEL("Stopping\n");
    AIOContinuousBufCleanup( buf );
    pthread_exit((void*)&retval);
  
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Main work function for collecting data. Also performs copies from 
 *       the raw acquiring buffer into the AIOContinuousBuf
 * @param object 
 * @return 
 * @todo Ensure that copying matches the actual size of the data
 */
void *ConvertCountsToVoltsFunction( void *object )
{
    AIORET_TYPE retval;
    int usbresult;
    AIOContinuousBuf *buf = (AIOContinuousBuf*)object;
    unsigned long result;
    int bytes;
    unsigned datasize = 64*1024;
    /* unsigned datasize = AIOContinuousBufNumberChannels(buf)*16*512; */

    int usbfail = 0, usbfail_count = 5;
    unsigned count = 0;
    int num_channels = AIOContinuousBufNumberChannels(buf);
    int num_oversamples = AIOContinuousBufGetOverSample(buf);
    int num_scans = AIOContinuousBufGetNumberScansToRead(buf);
    AIOFifoCounts *infifo = NewAIOFifoCounts( (unsigned)num_channels*(num_oversamples+1)*num_scans );
    AIOFifoVolts *outfifo = (AIOFifoVolts*)buf->fifo;


    AIOGainRange *ranges;
    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf), &result );
    unsigned char *data   = (unsigned char *)malloc( datasize );

    AIOCountsConverter *cc;
    AIOUSBDevice *dev = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf), &result );
    if ( result != AIOUSB_SUCCESS )
        goto out_ConvertCountsToVoltsFunction;
    AIOUSBDeviceGetADCConfigBlock( dev );

    ranges = NewAIOGainRangeFromADCConfigBlock( AIOUSBDeviceGetADCConfigBlock( dev ) );
    if ( !ranges )
        goto out_ConvertCountsToVoltsFunction;

    cc = NewAIOCountsConverterWithScanLimiter( (unsigned short*)data, num_scans, num_channels, ranges, num_oversamples , sizeof(unsigned short)  );
    if ( !cc ) 
        goto out_ConvertCountsToVoltsFunction;

    /**
     * @brief Load the fifo with values
     */
   
    while ( buf->status == RUNNING  ) {
        
        usbresult = aiocontbuf_get_data( buf, usb, 0x86, data, datasize, &bytes, 3000 );

        AIOUSB_DEVEL("Using counts=%d\n",bytes / 2 );

        bytes = MIN( (int)(buf->num_channels * (buf->num_oversamples+1)*buf->num_scans * sizeof(uint16_t) - count*sizeof(uint16_t)), bytes ); 

        retval = infifo->PushN( infifo, (uint16_t*)data, bytes / 2 );
        AIOUSB_DEVEL("Pushed %d bytes\n",retval );

        AIOUSB_DEVEL("libusb_bulk_transfer returned  %d as usbresult, bytes=%d\n", usbresult , (int)bytes);
        if ( bytes ) {
            /* only write bytes that exist */

            retval = cc->ConvertFifo( cc, outfifo, infifo , bytes / sizeof(uint16_t) );

            if (  retval >= 0 ) {
                count += retval;
            }


            AIOUSB_DEVEL("Pushed %d, size: %d\n", bytes / 2 , buf->fifo->size );
            AIOUSB_DEVEL("Tmpcount=%d,count=%d,Bytes=%d, Write=%d,Read=%d,max=%d\n", retval,count,bytes,get_write_pos(buf) , get_read_pos(buf),buffer_size(buf));

            /**
             * Modification, allow the count to keep going... stop 
             * if 
             * 1. count >= number we are supposed to read
             * 2. we don't have enough space
             */
            if ( count >= buf->num_scans*buf->num_channels ) {
                AIOContinuousBufLock(buf);
                buf->status = TERMINATED;
                AIOContinuousBufUnlock(buf);
            }
        } else if (  usbresult < 0  && usbfail < usbfail_count ) {
            AIOUSB_ERROR("Error with usb: %d\n", (int)usbresult );
            usbfail ++;
        } else {
            if (  usbfail >= usbfail_count  ){
                AIOUSB_ERROR("Erroring out. too many usb failures: %d\n", usbfail_count );
                retval = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT(usbresult);
                AIOContinuousBufLock(buf);
                buf->status = TERMINATED;
                AIOContinuousBufUnlock(buf);
                buf->exitcode = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT(usbresult);
            } 
        }
    }
 out_ConvertCountsToVoltsFunction:
    DeleteAIOFifoCounts(infifo);
    DeleteAIOCountsConverter( cc );
    free(data);
    AIOContinuousBufLock(buf);
    buf->status = TERMINATED;
    AIOContinuousBufUnlock(buf);
    AIOUSB_DEVEL("Stopping\n");
    AIOContinuousBufCleanup( buf );

    AIOUSB_ClearFIFO( AIOContinuousBufGetDeviceIndex(buf) ,   CLEAR_FIFO_METHOD_NOW );

    pthread_exit((void*)&retval);
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE StartStreaming( AIOContinuousBuf *buf )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIORESULT result = AIOUSB_SUCCESS;
    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );

    if ( result != AIOUSB_SUCCESS ) 
        return -AIOUSB_ERROR_INVALID_USBDEVICE;

    unsigned wValue = 0;
    unsigned wLength = 4;
    unsigned wIndex = 0;
    unsigned char data[] = {0x07, 0x0, 0x0, 0x1 } ;
    int usbval = usb->usb_control_transfer(usb, 
                                           USB_WRITE_TO_DEVICE, 
                                           AUR_START_ACQUIRING_BLOCK,
                                           wValue,
                                           wIndex,
                                           data,
                                           wLength,
                                           buf->timeout
                                           );
    if ( usbval < 0 ) {
        retval = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT(usbval );
    }
    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE SetConfig( AIOContinuousBuf *buf )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    unsigned long result;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    if ( result != AIOUSB_SUCCESS )
        return result;
    USBDevice *usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    if ( !usb )
        return AIOUSB_ERROR_INVALID_USBDEVICE;

    ADCConfigBlock *config = AIOUSBDeviceGetADCConfigBlock( deviceDesc );

    usb->usb_put_config( usb, config );

    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ResetCounters( AIOContinuousBuf *buf )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;

    AIORESULT result = AIOUSB_SUCCESS;
    unsigned wValue = 0x7400;
    unsigned wLength = 0;
    unsigned wIndex = 0;
    unsigned char data[0];
    int usbval;

    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    
    if ( result != AIOUSB_SUCCESS ) {
        goto out_ResetCounters;
    } else if ( !usb ) {
        result = AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
        goto out_ResetCounters;

    }

    usbval = usb->usb_control_transfer(usb, 
                                       USB_WRITE_TO_DEVICE, 
                                       AUR_CTR_MODE,
                                       wValue,
                                       wIndex,
                                       data,
                                       wLength,
                                       buf->timeout
                                       );
    if ( usbval  != 0 ) {
        retval = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT(usbval);
        goto out_ResetCounters;
    }
    wValue = 0xb600;
    usbval = usb->usb_control_transfer(usb,
                                       USB_WRITE_TO_DEVICE, 
                                       AUR_CTR_MODE,
                                       wValue,
                                       wIndex,
                                       data,
                                       wLength,
                                       buf->timeout
                                       );
    if ( usbval  != 0 )
        retval = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT(usbval);
 out_ResetCounters:
    AIOUSB_UnLock();
    return retval;

}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufLoadCounters( AIOContinuousBuf *buf, unsigned countera, unsigned counterb )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIORESULT result = AIOUSB_SUCCESS;

    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    if ( result != AIOUSB_SUCCESS ) 
        return -result;

    unsigned wValue = 0x7400;
    unsigned wLength = 0;
    unsigned char data[0];
    unsigned timeout = 3000;

    int usbval = usb->usb_control_transfer(usb,
                                           USB_WRITE_TO_DEVICE, 
                                           AUR_CTR_MODELOAD,
                                           wValue,
                                           countera,
                                           data,
                                           wLength,
                                           timeout
                                           );
    if ( usbval != 0 ) {
        retval = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT(usbval);
        goto out_AIOContinuousBufLoadCounters;
    }
    wValue = 0xb600;
    usbval = usb->usb_control_transfer(usb,
                                       USB_WRITE_TO_DEVICE, 
                                       AUR_CTR_MODELOAD,
                                       wValue,
                                       counterb,
                                       data,
                                       wLength,
                                       timeout
                                       );
    if ( usbval != 0 )
        retval = -(AIORET_TYPE)LIBUSB_RESULT_TO_AIOUSB_RESULT(usbval);

out_AIOContinuousBufLoadCounters:
    return retval;
}

int continuous_end( USBDevice *usb , unsigned char *data, unsigned length )
{
    int retval = 0;
    unsigned bmRequestType, wValue = 0x0, wIndex = 0x0, bRequest = 0xba, wLength = 0x01;

    /* 40 BC 00 00 00 00 04 00 */
    bmRequestType = 0x40;
    bRequest = 0xbc;
    wLength = 4;
    data[0] = 0x2;
    data[1] = 0;
    data[2] = 0x2;
    data[3] = 0;
    usb->usb_control_transfer( usb,
                               bmRequestType,
                               bRequest,
                               wValue,
                               wIndex,
                               &data[0],
                               wLength,
                               1000
                               );

    /* C0 BC 00 00 00 00 04 00 */
    bmRequestType = 0xc0;
    bRequest = 0xbc;
    usb->usb_control_transfer( usb,
                               bmRequestType,
                               bRequest,
                               wValue,
                               wIndex,
                               &data[0],
                               wLength,
                               1000
                               );


    /* 40 21 00 74 00 00 00 00 */
    bmRequestType = 0x40;
    bRequest = 0x21;
    wValue = 0x7400;
    wLength = 0;
    wIndex = 0;
    usb->usb_control_transfer( usb,
                               bmRequestType,
                               bRequest,
                               wValue,
                               wIndex,
                               &data[0],
                               wLength,
                               1000
                               );
    
    wValue = 0xb600;
    usb->usb_control_transfer( usb,
                               bmRequestType,
                               bRequest,
                               wValue,
                               wIndex,
                               &data[0],
                               wLength,
                               1000
                               );

    return retval;
}


AIORET_TYPE AIOContinuousBufCleanup( AIOContinuousBuf *buf )
{
    AIORET_TYPE retval;
    unsigned char data[4] = {0};
    AIORESULT result = AIOUSB_SUCCESS;

    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    if ( result != AIOUSB_SUCCESS )
        return -result;
    
    retval = (AIORET_TYPE)continuous_end( usb, data, 4 );
    return retval;
}

AIORET_TYPE AIOContinuousBufPreSetup( AIOContinuousBuf * buf )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    int usbval;
    AIORESULT result;
    unsigned char data[0];
    unsigned wLength = 0;
    int wValue  = 0x7400, wIndex = 0;
    unsigned timeout = 7000;
    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    if (result != AIOUSB_SUCCESS ) {
        retval = -result;
        goto out_AIOContinuousBufPreSetup;
    }

    /* Write 02 00 02 00 */
    /* 40 bc 00 00 00 00 04 00 */
  
    usbval = usb->usb_control_transfer( usb, 
                                        USB_WRITE_TO_DEVICE,
                                        AUR_CTR_MODE,
                                        wValue,
                                        wIndex,
                                        data,
                                        wLength,
                                        timeout
                                        );
    if (  usbval != AIOUSB_SUCCESS ) {
        retval = -usbval;
        goto out_AIOContinuousBufPreSetup;
    }
    wValue = 0xb600;

    /* Read c0 bc 00 00 00 00 04 00 */ 
    usbval = usb->usb_control_transfer( usb,
                                        USB_WRITE_TO_DEVICE,
                                        AUR_CTR_MODE,
                                        wValue,
                                        wIndex,
                                        data,
                                        wLength,
                                        timeout
                                      );
    if (  usbval != 0 )
        retval = -usbval;

 out_AIOContinuousBufPreSetup:
    return retval;

}

/*----------------------------------------------------------------------------*/
int continuous_setup( USBDevice *usb , unsigned char *data, unsigned length )
{
    unsigned bmRequestType, wValue = 0x0, wIndex = 0x0, bRequest = 0xba, wLength = 0x01;
    unsigned tmp[] = {0xC0, 0xBA, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00};
    memcpy(data,tmp, 8);
    int usbval = usb->usb_control_transfer( usb,
                                            0xC0,
                                            bRequest,
                                            wValue,
                                            wIndex,
                                            &data[0],
                                            wLength,
                                            1000
                                            );
    wValue = 0;
    wIndex = 0;
    wLength = 0x14;
    memset(data,(unsigned char)1,16);
    data[16] = 0;
    data[17] = 0x15;
    data[18] = 0xf0;
    data[19] = 0;
    /* 40 21 00 74 00 00 00 00 */
    bmRequestType = 0x40;
    bRequest = 0x21;
    wValue = 0x7400;
    wIndex =0;
    wLength = 0;
    usb->usb_control_transfer( usb,
                               bmRequestType,
                               bRequest,
                               wValue,
                               wIndex,
                               &data[0],
                               wLength,
                               1000
                               );
    /* 40 21 00 B6 00 00 00 00 */
    wValue = 0xB600;
    usb->usb_control_transfer( usb,
                               bmRequestType,
                               bRequest,
                               wValue,
                               wIndex,
                               &data[0],
                               wLength,
                               1000
                               );
    /*Config */


    /* 40 23 00 74 25 00 00 00 */
    wValue = 0x7400;
    bRequest = 0x23;
    wIndex = 0x64;
    usb->usb_control_transfer( usb,
                               bmRequestType,
                               bRequest,
                               wValue,
                               wIndex,
                               &data[0],
                               wLength,
                               1000
                               );


    /* 40 23 00 B6 64 00 00 00 */
    wValue = 0xb600;
    bRequest = 0x23;
    wIndex = 0x64;
    usb->usb_control_transfer( usb,
                               bmRequestType,
                               bRequest,
                               wValue,
                               wIndex,
                               &data[0],
                               wLength,
                               1000
                               );



    /* 40 BC 00 00 00 00 04 00 */
    data[0] = 0x07;
    data[1] = 0x0;
    data[2] = 0x0;
    data[3] = 0x01;
    wValue = 0x0;
    wIndex = 0x0;
    wLength = 4;
    bRequest = 0xBC;
    usb->usb_control_transfer( usb,
                               bmRequestType,
                               bRequest,
                               wValue,
                               wIndex,
                               &data[0],
                               wLength,
                               1000
                               );
    return usbval;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Setups the Automated runs for continuous mode runs
 * @param buf 
 * @return 
 */
AIORET_TYPE AIOContinuousBufCallbackStart( AIOContinuousBuf *buf )
{
    AIORET_TYPE retval;
    /**
     * @note Setup counters
     * see reference in [USB AIO documentation](http://accesio.com/MANUALS/USB-AIO%20Series.PDF)
     **/

    /* Start the clocks, and need to get going capturing data */
    if ( (retval = ResetCounters(buf)) != AIOUSB_SUCCESS )
        goto out_AIOContinuousBufCallbackStart;

    /* AIOUSB_ClearFIFO( AIOContinuousBufGetDeviceIndex(buf) ,   CLEAR_FIFO_METHOD_NOW ); */

    if ( (retval = SetConfig(buf)) != AIOUSB_SUCCESS )
        goto out_AIOContinuousBufCallbackStart;
    if ( (retval = CalculateClocks( buf ) ) != AIOUSB_SUCCESS )
        goto out_AIOContinuousBufCallbackStart;
    /* Try a switch */
    if ( (retval = StartStreaming(buf)) != AIOUSB_SUCCESS )
        goto out_AIOContinuousBufCallbackStart;

    /**
     * @note BufStart ( or bulk read ) must occur before loading the counters
     */ 
    retval = AIOContinuousBufStart( buf ); /* Startup the thread that handles the data acquisition */

    if ( ( retval = AIOContinuousBufLoadCounters( buf, buf->divisora, buf->divisorb )) != AIOUSB_SUCCESS)
        goto out_AIOContinuousBufCallbackStart;



    if (  retval != AIOUSB_SUCCESS )
        goto cleanup_AIOContinuousBufCallbackStart;
    /**
     * Allow the other command to be run
     */
 out_AIOContinuousBufCallbackStart:
    return retval;
 cleanup_AIOContinuousBufCallbackStart:
    AIOContinuousBufCleanup( buf );
    return retval;
}

AIORET_TYPE AIOContinuousBuf_ResetDevice(AIOContinuousBuf *buf ) 
{
    return AIOContinuousBufResetDevice( buf );
}

AIORET_TYPE AIOContinuousBufResetDevice( AIOContinuousBuf *buf) 
{
    unsigned char data[2] = {0x01};
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIORESULT result = AIOUSB_SUCCESS;
    int usbval;
    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    if ( result != AIOUSB_SUCCESS ) {
        retval = -result;
        goto out_AIOContinuousBuf_ResetDevice;
    }
  
    usbval = usb->usb_control_transfer(usb, 0x40, 0xA0, 0xE600, 0 , data, 1, buf->timeout );
    data[0] = 0;

    usbval = usb->usb_control_transfer(usb, 0x40, 0xA0, 0xE600, 0 , data, 1, buf->timeout );
    retval = (AIORET_TYPE )usbval;
 out_AIOContinuousBuf_ResetDevice:
    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Reads the current available amount of data from buf, into 
 *       the readbuf datastructure
 * @param buf 
 * @param readbuf 
 * @return If number is positive, it is the number of bytes that have been read.
 */
AIORET_TYPE AIOContinuousBufRead( AIOContinuousBuf *buf, AIOBufferType *readbuf , unsigned readbufsize, unsigned size)
{

    AIORET_TYPE retval;

    AIOContinuousBufLock( buf );
    int N = MIN(size ,readbufsize ) / buf->fifo->refsize;

    retval = buf->fifo->PopN( buf->fifo, readbuf, N );

    retval = ( retval == 0 ? -AIOUSB_ERROR_NOT_ENOUGH_MEMORY : retval );

    AIOContinuousBufUnlock( buf );
    return retval;
}


/*----------------------------------------------------------------------------*/
/**
 * @brief 
 * @param buf 
 * @return 
 */
AIORET_TYPE AIOContinuousBufLock( AIOContinuousBuf *buf )
{
    AIORET_TYPE retval = 0;
#ifdef HAS_PTHREAD
    retval = pthread_mutex_lock( &buf->lock );
    if ( retval != 0 ) {
        retval = -retval;
    }
#endif
    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufUnlock( AIOContinuousBuf *buf )
{
    int retval = 0;
#ifdef HAS_PTHREAD
    retval = pthread_mutex_unlock( &buf->lock );
    if ( retval !=  0 ) {
        retval = -retval; 
        AIOUSB_ERROR("Unable to unlock mutex");
    }
#endif
    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufSimpleSetupConfig( AIOContinuousBuf *buf, ADGainCode gainCode )
{
    AIORET_TYPE retval;
    ADCConfigBlock configBlock = {0};
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf) , &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    ADCConfigBlockInit( &configBlock, deviceDesc, AIOUSB_FALSE );

    ADCConfigBlockSetAllGainCodeAndDiffMode( &configBlock, gainCode, AIOUSB_FALSE );
    ADCConfigBlockSetTriggerMode( &configBlock, AD_TRIGGER_SCAN | AD_TRIGGER_TIMER ); /* 0x05 */
    ADCConfigBlockSetScanRange( &configBlock, 0, 15 ); /* All 16 channels */

    ADC_QueryCal( AIOContinuousBufGetDeviceIndex(buf) );
    retval = ADC_SetConfig( AIOContinuousBufGetDeviceIndex(buf), configBlock.registers, &configBlock.size );
    if ( retval != AIOUSB_SUCCESS ) 
        return (AIORET_TYPE)(-retval);
    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufEnd( AIOContinuousBuf *buf )
{ 
    void *ptr;
    AIORET_TYPE ret;
    AIOContinuousBufLock( buf );

    AIOUSB_DEVEL("Locking and finishing thread\n");

    buf->status = TERMINATED;
    AIOUSB_DEVEL("\tWaiting for thread to terminate\n");
    AIOUSB_DEVEL("Set flag to FINISH\n");
    AIOContinuousBufUnlock( buf );


#ifdef HAS_PTHREAD
    ret = pthread_join( buf->worker , &ptr );
#endif
    if ( ret != 0 ) {
        AIOUSB_ERROR("Error joining threads");
    }
    buf->status = JOINED;
    return ret;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_SetTesting( AIOContinuousBuf *buf, AIOUSB_BOOL testing ) {return AIOContinuousBufSetTesting( buf, testing );}
AIORET_TYPE AIOContinuousBufSetTesting( AIOContinuousBuf *buf, AIOUSB_BOOL testing )
{
    if ( !buf )
        return -AIOUSB_ERROR_INVALID_AIOCONTINUOUS_BUFFER;

    AIOContinuousBufLock( buf );
    /* ADC_SetTestingMode( AIOUSB_GetConfigBlock( AIOContinuousBuf_GetDeviceIndex(buf)), testing ); */
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf), &result );
    if ( result != AIOUSB_SUCCESS ) 
        goto out_AIOContinuousBufSetTesting;

    result = AIOUSBDeviceSetTesting( device, testing );
    if ( result != AIOUSB_SUCCESS )
        goto out_AIOContinuousBufSetTesting;

    result = ADCConfigBlockSetTesting( AIOUSBDeviceGetADCConfigBlock( device ), testing );
    if ( result != AIOUSB_SUCCESS )
        goto  out_AIOContinuousBufSetTesting;

    buf->testing = testing;
 out_AIOContinuousBufSetTesting:
    AIOContinuousBufUnlock( buf );
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufGetTesting( AIOContinuousBuf *buf )
{
    if ( !buf )
        return -AIOUSB_ERROR_INVALID_AIOCONTINUOUS_BUFFER;
    return buf->testing;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufSetDebug( AIOContinuousBuf *buf, AIOUSB_BOOL debug )
{
    if ( !buf )
        return -AIOUSB_ERROR_INVALID_AIOCONTINUOUS_BUFFER;

    AIOContinuousBufLock( buf );
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf), &result );
    if ( result != AIOUSB_SUCCESS )
        return -result;

    result = ADCConfigBlockSetDebug( AIOUSBDeviceGetADCConfigBlock( device ), debug );
    if ( result != AIOUSB_SUCCESS )
        return -result;

    buf->debug = debug;
    AIOContinuousBufUnlock( buf );
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBufGetDebug( AIOContinuousBuf *buf )
{
    if ( !buf )
        return -AIOUSB_ERROR_INVALID_AIOCONTINUOUS_BUFFER;
    return buf->debug;
}



/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_SetDeviceIndex( AIOContinuousBuf *buf , unsigned long DeviceIndex ) { return AIOContinuousBufSetDeviceIndex( buf, DeviceIndex ); }
AIORET_TYPE AIOContinuousBufSetDeviceIndex( AIOContinuousBuf *buf , unsigned long DeviceIndex )
{
    AIOContinuousBufLock( buf );
    buf->DeviceIndex = DeviceIndex; 
    AIOContinuousBufUnlock( buf );
    return AIOUSB_SUCCESS;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_SaveConfig( AIOContinuousBuf *buf ) { return AIOContinuousBufSaveConfig(buf); }
AIORET_TYPE AIOContinuousBufSaveConfig( AIOContinuousBuf *buf ) 
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;

    SetConfig( buf );

    return retval;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_SetStartAndEndChannel( AIOContinuousBuf *buf, 
                                                    unsigned startChannel, 
                                                    unsigned endChannel ) {
    return AIOContinuousBufSetStartAndEndChannel( buf, startChannel, endChannel );
}
AIORET_TYPE AIOContinuousBufSetStartAndEndChannel( AIOContinuousBuf *buf, unsigned startChannel, unsigned endChannel )
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf), &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return -result;
    }
    if ( AIOContinuousBufNumberChannels( buf ) > 16 ) {
        deviceDesc->cachedConfigBlock.size = AD_MUX_CONFIG_REGISTERS;
    }

    return -(AIORET_TYPE)abs(ADCConfigBlockSetScanRange( AIOUSBDeviceGetADCConfigBlock( deviceDesc ) , startChannel, endChannel ));
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_SetChannelRangeGain( AIOContinuousBuf *buf, 
                                                  unsigned startChannel, 
                                                  unsigned endChannel , 
                                                  unsigned gainCode ) { return AIOContinuousBufSetChannelRange(buf,startChannel,endChannel, gainCode ); }

AIORET_TYPE AIOContinuousBuf_SetChannelRange( AIOContinuousBuf *buf, 
                                              unsigned startChannel, 
                                              unsigned endChannel , 
                                              unsigned gainCode ) { return AIOContinuousBufSetChannelRange(buf,startChannel,endChannel, gainCode ); }
AIORET_TYPE AIOContinuousBufSetChannelRange( AIOContinuousBuf *buf, 
                                             unsigned startChannel, 
                                             unsigned endChannel , 
                                             unsigned gainCode )
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf) , &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    for ( unsigned i = startChannel; i <= endChannel ; i ++ ) {
#ifdef __cplusplus
        ADCConfigBlockSetGainCode( AIOUSBDeviceGetADCConfigBlock( deviceDesc ), i, static_cast<ADGainCode>(gainCode));
#else
        ADCConfigBlockSetGainCode( AIOUSBDeviceGetADCConfigBlock( deviceDesc ), i, gainCode);
#endif
    }
    return result;
}

/*----------------------------------------------------------------------------*/
PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufSetTimeout( AIOContinuousBuf *buf, unsigned timeout )
{
    assert(buf);
    AIOContinuousBufLock( buf );
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *dev = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    if ( result != AIOUSB_SUCCESS ) 
        return -AIOUSB_ERROR_INVALID_DEVICE_SETTING;

    retval = AIOUSBDeviceSetTimeout( dev, timeout );
    if ( retval != AIOUSB_SUCCESS )
        return retval;
    buf->timeout = timeout;

    AIOContinuousBufUnlock( buf );
    return retval;
}

PUBLIC_EXTERN AIORET_TYPE AIOContinuousBufGetTimeout( AIOContinuousBuf *buf )
{
    assert(buf);
    if (!buf )
        return -AIOUSB_ERROR_INVALID_DEVICE;
    
    return buf->timeout;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_SetOverSample( AIOContinuousBuf *buf, unsigned os ) { return AIOContinuousBufSetOverSample(buf,os);}
AIORET_TYPE AIOContinuousBufSetOverSample( AIOContinuousBuf *buf, unsigned os )
{
    assert(buf);
    AIOContinuousBufLock( buf );
    AIORESULT result = AIOUSB_SUCCESS;
    AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    if ( result != AIOUSB_SUCCESS )  {
        result = -AIOUSB_ERROR_INVALID_DEVICE_SETTING;
        goto fail;
    }

    result = ADC_SetOversample( AIOContinuousBufGetDeviceIndex(buf), os );     
 fail:
    AIOContinuousBufUnlock( buf );
    return result;
}

/*------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_GetOverSample( AIOContinuousBuf *buf ) { return AIOContinuousBufGetOverSample( buf ); }
AIORET_TYPE AIOContinuousBufGetOverSample( AIOContinuousBuf *buf ) {
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *device = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf), &result );
    if ( result != AIOUSB_SUCCESS )
        return -result;

    return ADCConfigBlockGetOversample( AIOUSBDeviceGetADCConfigBlock( device ) );
}

/*------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_SetAllGainCodeAndDiffMode( AIOContinuousBuf *buf, ADGainCode gain, AIOUSB_BOOL diff ) {
    return AIOContinuousBufSetAllGainCodeAndDiffMode( buf, gain, diff );
}
AIORET_TYPE AIOContinuousBufSetAllGainCodeAndDiffMode( AIOContinuousBuf *buf, ADGainCode gain, AIOUSB_BOOL diff )
{
    AIOContinuousBufLock( buf );
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *dev = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex( buf ), &result );
    if ( result != AIOUSB_SUCCESS ) 
        goto out_AIOContinuousBufSetAllGainCodeAndDiffMode;

    result = ADCConfigBlockSetAllGainCodeAndDiffMode( AIOUSBDeviceGetADCConfigBlock( dev ), gain, diff );

 out_AIOContinuousBufSetAllGainCodeAndDiffMode:
    AIOContinuousBufUnlock( buf );
    return result;
}

AIORET_TYPE AIOContinuousBuf_SetDiscardFirstSample(  AIOContinuousBuf *buf , AIOUSB_BOOL discard ){ return AIOContinuousBufSetDiscardFirstSample( buf, discard ); }
AIORET_TYPE AIOContinuousBufSetDiscardFirstSample(  AIOContinuousBuf *buf , AIOUSB_BOOL discard ) 
{
    AIOContinuousBufLock( buf );
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *dev = AIODeviceTableGetDeviceAtIndex( AIOContinuousBufGetDeviceIndex(buf), &result );
    if ( result != AIOUSB_SUCCESS ) 
        goto out_AIOContinuousBufSetDiscardFirstSample;
    
    dev->discardFirstSample = discard;

 out_AIOContinuousBufSetDiscardFirstSample:
    AIOContinuousBufUnlock( buf );
    return result;
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOContinuousBuf_GetDeviceIndex( AIOContinuousBuf *buf ) {return AIOContinuousBufGetDeviceIndex( buf ); }
AIORET_TYPE AIOContinuousBufGetDeviceIndex( AIOContinuousBuf *buf )
{
    if ( buf->DeviceIndex < 0 )
        return -AIOUSB_ERROR_DEVICE_NOT_FOUND;

    return (AIORET_TYPE)buf->DeviceIndex;
}


#ifdef __cplusplus
}
#endif

/*****************************************************************************
 * Self-test 
 * @note This section is for stress testing the Continuous buffer in place
 * without using the USB features
 *
 ****************************************************************************/ 

#ifdef SELF_TEST



#include "AIOUSBDevice.h"
#include "gtest/gtest.h"
#include "tap.h"
#include <iostream>
using namespace AIOUSB;



#ifndef TAP_TEST
#define LOG(...) do {                           \
    pthread_mutex_lock( &message_lock );        \
    printf( __VA_ARGS__ );                      \
    pthread_mutex_unlock(&message_lock);        \
  } while ( 0 );
#else
#define LOG(...) do { } while (0); 
#endif


void fill_buffer( AIOBufferType *buffer, unsigned size )
{
  for ( int i = 0 ; i < size; i ++ ) { 
    buffer[i] = rand() % 1000;
  }
}

void *newdoit(void *object )
{
  int counter = 0;
  AIORET_TYPE retval = AIOUSB_SUCCESS;
  while ( counter < 10 ) {
    AIOUSB_DEBUG("Waiting in thread counter=%d\n", counter );
    sleep(1);
    counter++;
  }
  pthread_exit((void*)&retval);
  return (void*)NULL;
}

/** 
 * @param object 
 * @return 
 */
void *doit( void *object )
{
    sched_yield();
    AIOContinuousBuf *buf = (AIOContinuousBuf*)object;
    AIOUSB_DEVEL("\tAddress is 0x%x\n", (int)(unsigned long)(AIOContinuousBuf *)buf );
    unsigned  size  = 1000;
    AIOBufferType *tmp = (AIOBufferType*)malloc(size*sizeof(AIOBufferType));
    AIORET_TYPE retval;

    while ( buf->status == RUNNING ) { 
        fill_buffer( tmp, size );
        AIOUSB_DEVEL("\tLooping spinning wheels\n"); 
        retval = AIOContinuousBufWrite( buf, tmp, size, size , AIOCONTINUOUS_BUF_NORMAL );
        AIOUSB_DEVEL("\tWriting buf , attempted write of size size=%d, wrote=%d\n", size, (int)retval );
    }
    AIOUSB_DEVEL("Stopping\n");
    AIOUSB_DEVEL("Completed loop\n");
    free(tmp);
    pthread_exit((void*)&retval);
    return NULL;
}

/** 
 * @param object 
 * @return 
 */
void *channel16_doit( void *object )
{
    sched_yield();
    AIOContinuousBuf *buf = (AIOContinuousBuf*)object;
    AIOUSB_DEVEL("\tAddress is 0x%x\n", (int)(unsigned long)(AIOContinuousBuf *)buf );
    unsigned  size  = 16*64;
    AIOBufferType *tmp = (AIOBufferType*)malloc(size*sizeof(AIOBufferType));
    AIORET_TYPE retval;

    while ( buf->status == RUNNING ) { 
        fill_buffer( tmp, size );
        AIOUSB_DEVEL("\tLooping spinning wheels\n"); 
        retval = AIOContinuousBufWrite( buf, tmp, size ,size,  AIOCONTINUOUS_BUF_ALLORNONE );
        usleep( rand()%100 );
        if (  retval >= 0 && retval != size ) {
            AIOUSB_ERROR("Error writing. Wrote bytes of size=%d but should have written=%d\n", (int)retval, size );
            AIOUSB_ERROR("read_pos=%d, write_pos=%d\n", get_read_pos(buf), get_write_pos(buf));
            _exit(2);
        }
        AIOUSB_DEVEL("\tWriting buf , attempted write of size size=%d, wrote=%d\n", size, (int)retval );
    }
    AIOUSB_DEVEL("Stopping\n");
    AIOUSB_DEVEL("Completed loop\n");
    free(tmp);
    pthread_exit((void*)&retval);
    return NULL;
}

void
stress_test_one( int size , int readbuf_size )
{
    AIORET_TYPE retval;
    /* int readbuf_size = size - 10; */
    AIOBufferType *readbuf = (AIOBufferType *)malloc( readbuf_size*sizeof(AIOBufferType ));
    AIOContinuousBuf *buf = NewAIOContinuousBuf( 0, size , 16 );
    AIOUSB_DEVEL("Original address is 0x%x\n", (int)(unsigned long)(AIOContinuousBuf *)buf );
    AIOContinuousBufReset( buf );
    AIOContinuousBufSetCallback( buf , doit );
    AIOUSB_DEBUG("Was able to reset device\n");
    retval = AIOContinuousBufStart( buf );
    AIOUSB_DEBUG("Able to start new Acquisition\n");
    EXPECT_GT( retval, -1 );

    for(int i = 0 ; i < 500; i ++ ) {
        /* retval = AIOContinuousBufRead( buf,  readbuf, readbuf_size ); */
        retval = AIOContinuousBufRead( buf,  readbuf, readbuf_size, readbuf_size );
        usleep(rand() % 100);
        AIOUSB_DEVEL("Read number of bytes=%d\n",(int)retval );
    }
    AIOContinuousBufEnd( buf );
    int distance = ( get_read_pos(buf) > get_write_pos(buf) ? 
                     (buffer_size(buf) - 1 - get_read_pos(buf) ) + get_write_pos(buf) :
                     get_write_pos(buf) - get_read_pos(buf) );
    
    AIOUSB_DEVEL("Read: %d, Write: %d\n", get_read_pos(buf),get_write_pos(buf));
    for( int i = 0; i <= distance / readbuf_size ; i ++ ) {
        retval = AIOContinuousBufRead( buf, readbuf,readbuf_size,readbuf_size  );
    }
    retval = AIOContinuousBufRead( buf, readbuf, readbuf_size ,readbuf_size );
    EXPECT_EQ( retval, 0 )  << "Couldn't read in the entire buffer for size " << readbuf_size << std::endl;

    DeleteAIOContinuousBuf( buf );
    free(readbuf);
}


void stress_test_read_channels( int bufsize, int keysize  ) 
{
    AIOContinuousBuf *buf = NewAIOContinuousBuf( 0,  bufsize , 16 );
    int mybufsize = (16*keysize);
    int stopval;
    AIOBufferType *tmp = (AIOBufferType *)malloc(mybufsize*sizeof(AIOBufferType ));
    AIORET_TYPE retval;
    AIOContinuousBufSetCallback( buf , channel16_doit);
    AIOContinuousBufReset( buf );
    retval = AIOContinuousBufStart( buf );
    if (  retval < AIOUSB_SUCCESS )
        goto out_stress_test_read_channels;

    for ( int i = 0; i < 2000; i ++ ) {
        retval = AIOContinuousBufRead( buf, tmp, mybufsize, mybufsize );
        AIOUSB_DEVEL("Read %d bytes\n", (int)retval );
        usleep(rand()%100);
        if ( retval < AIOUSB_SUCCESS )
            goto out_stress_test_read_channels;
    }
    AIOContinuousBufEnd( buf );
    /* Now read out all of the remaining sizes */
    stopval =read_size(buf) / mybufsize;
    if (  stopval == 0 )
        stopval = 1;
    for( int i = 1 ; i <= stopval ; i ++ ) {
        retval = AIOContinuousBufRead( buf, tmp, mybufsize, mybufsize );
    }
    retval = AIOContinuousBufRead( buf, tmp, mybufsize, mybufsize );

 out_stress_test_read_channels:
  
    EXPECT_EQ( retval, AIOUSB_SUCCESS );

    free(tmp);
    DeleteAIOContinuousBuf( buf );
}


void continuous_stress_test( int bufsize )
{
    AIOContinuousBuf *buf = NewAIOContinuousBuf( 0, bufsize , 16 );
    int tmpsize = pow(16,(double)ceil( ((double)log((double)(bufsize/1000))) / log(16)));
    int keepgoing = 1;
    AIORET_TYPE retval;
    AIOBufferType *tmp = (AIOBufferType *)malloc(sizeof(AIOBufferType *)*tmpsize);
    int ntest_count = 0;

    AIOUSB_Init();
    GetDevices();
    AIOContinuousBufSetClock( buf, 1000 );
    AIOContinuousBufCallbackStart( buf );

    while ( keepgoing ) {
        retval = AIOContinuousBufRead( buf, tmp, tmpsize, tmpsize );
        sleep(1);
        AIOUSB_INFO("Waiting : readpos=%d, writepos=%d\n", get_read_pos(buf),get_write_pos(buf));
        if (  get_read_pos(buf) < 1000 ) {
            ntest_count ++;
        }
#ifdef NTEST
        if (  ntest_count > 5000 ) {
            AIOContinuousBufEnd( buf );
            keepgoing = 0;
        }
#else
        if (  get_read_pos( buf )  > 60000 ) {
            AIOContinuousBufEnd( buf );
            keepgoing = 0;
        }
#endif
    }
#ifdef TESTING
    set_read_pos(buf,0);
    for(int i = 0; i < get_write_pos(buf) /16 ; i ++ ) {
        for( int j =0; j < 16 ; j ++ ) { 
            printf("%f,", buf->buffer[i*16+j] );
        }
        printf("\n");
    }
#endif

    ASSERT_GE( retval, AIOUSB_SUCCESS ) << "Able to finish reading buffer\n";
}

AIORET_TYPE read_data( unsigned short *data , unsigned size) 
{
  
  for ( int i = 0 ; i < size; i ++ ) { 
    data[i] = i % 256;
  }
  return (AIORET_TYPE)size;
}

/* 
 * Dummy setup
 */
void dummy_init(void)
{
    int numAccesDevices = 0;
    aiousbInit = AIOUSB_INIT_PATTERN;
    AIOUSB_Init();
    AIORESULT result = AIOUSB_SUCCESS;
    AIODeviceTableAddDeviceToDeviceTableWithUSBDevice( &numAccesDevices, USB_AIO12_128E, NULL );
    AIOUSBDevice *device = AIODeviceTableGetDeviceAtIndex( numAccesDevices ,  &result );

}

void stress_copy_counts (int bufsize) 
{
    unsigned char *data = (unsigned char *)malloc( bufsize ); /* Represents USB transaction, smaller in size */
    unsigned short *usdata = (unsigned short *)&data[0];
    unsigned short tobuf[32768] = {0};

    AIOContinuousBuf *buf = NewAIOContinuousBufTesting(0, bufsize , 16 , AIOUSB_TRUE ); /* Num channels is 16*bufsize  */
    AIORET_TYPE retval;
    int failed = 0;

    /* setup the data */
    memset(data,0,bufsize);

    for ( int i = 0; i < 32768 ;) { 
        for ( int ch = 0; ch < 16 && i < 32768 ; ch ++ , i ++ ) { 
            usdata[i] = (ch*20)+rand()%20;
        }
    }

    set_write_pos( buf, 16 );
    set_read_pos( buf, 0 );

    EXPECT_EQ(1,AIOContinuousBufCountScansAvailable(buf)) << "Minimum size is not correct\n";

    set_write_pos(buf, 0 );
    set_read_pos(buf,AIOContinuousBufNumberChannels(buf));

    EXPECT_EQ( write_size(buf), AIOContinuousBufNumberChannels(buf) ) << " write space left was not correct\n";
    EXPECT_EQ( bufsize*16, buffer_size(buf));
  
    set_read_pos(buf,0);

    retval = AIOContinuousBufWriteCounts( buf, usdata, bufsize/2, bufsize/2  , AIOCONTINUOUS_BUF_ALLORNONE );
    EXPECT_GE( retval, 0 ) << "Cant copy counts correctly\n";

    EXPECT_EQ( bufsize / 2 / AIOContinuousBufNumberChannels(buf), AIOContinuousBufCountScansAvailable(buf) ) << "Not the correct number of counts available";

    if (  AIOContinuousBufCountScansAvailable(buf)  ) { 
        retval = AIOContinuousBufReadIntegerScanCounts( buf, tobuf , 32768, AIOContinuousBufNumberChannels(buf)-1 );
        EXPECT_EQ( -AIOUSB_ERROR_NOT_ENOUGH_MEMORY, retval );
    }


    while (  AIOContinuousBufCountScansAvailable(buf)  && !failed ) {
        retval = AIOContinuousBufReadIntegerScanCounts( buf, tobuf , 32768, 32768 );
        ASSERT_GE( retval, AIOUSB_SUCCESS ) << "Unable to read from buffer at position " << (int)AIOContinuousBufGetReadPosition(buf) << std::endl;
        for( int i = 0, ch = 0 ; i < retval; i ++, ch = ((ch+1)% AIOContinuousBufNumberChannels(buf)) ) {
            ASSERT_EQ( tobuf[i], usdata[i] ) << "Did not get matching data for i=" << i << std::endl;
        }
    }
    EXPECT_FALSE( failed ) << "Did not get matching data";

    /* now try writing past the end */
    int i;
    int total_write = write_size (buf) / ( bufsize / (AIOContinuousBufNumberChannels(buf) ));

    for ( i = 0; i < total_write + 2; i ++ ) {
        AIOContinuousBufWriteCounts( buf, usdata, bufsize/2, bufsize/2 , AIOCONTINUOUS_BUF_OVERRIDE );
    }
  
    /* Read=0,Write=16384,size=4000000,Avail=4096; */
    DeleteAIOContinuousBuf(buf);
    /* --buffersize 1000000 --numchannels 16  --clockrate 10000; */
    buf = NewAIOContinuousBufTesting( 0, 1000000, 16 , AIOUSB_TRUE );
    /* set_write_pos(buf, 16384 ); */
    memset(usdata,0,bufsize/2);
    AIOContinuousBufWriteCounts( buf, usdata, bufsize/2,bufsize/2 , AIOCONTINUOUS_BUF_OVERRIDE );
    failed = 0;

    while (  AIOContinuousBufCountScansAvailable(buf)  && !failed ) {

        retval = AIOContinuousBufReadIntegerScanCounts( buf, tobuf , 32768, 32768 );
        ASSERT_GE( retval, AIOUSB_SUCCESS ) << "Couldn't read from buffer at position " << (int)AIOContinuousBufGetReadPosition(buf) << std::endl;

        unsigned short *tmpbuf = (unsigned short *)&tobuf[0];
        for( int i = 0, ch = 0 ; i < retval; i ++, ch = ((ch+1)% AIOContinuousBufNumberChannels(buf)) ) {
            ASSERT_EQ( tobuf[i], usdata[i] );
        }
    }
    DeleteAIOContinuousBuf(buf);
    buf = NewAIOContinuousBufTesting( 0, 10, 16 , AIOUSB_TRUE );
    retval = AIOContinuousBufWriteCounts( buf, usdata, bufsize/2,bufsize/2 , AIOCONTINUOUS_BUF_ALLORNONE ); 

    EXPECT_LE( retval, 0 ) << "Unable to prevent writes when not enough space";

    unsigned tmpsize = MIN( AIOContinuousBufNumberWriteScansInCounts(buf) , bufsize/2 );
    retval = AIOContinuousBufWriteCounts( buf, usdata, bufsize/2,tmpsize , AIOCONTINUOUS_BUF_ALLORNONE ); 
    
    EXPECT_EQ( tmpsize, retval ) << "Not able to write just until the end of the buffer\n";

    DeleteAIOContinuousBuf(buf); 
    free(data);
}

int bufsize = 1000;

class AIOContinuousBufSetup : public ::testing::Test 
{
 protected:
    virtual void SetUp() {
        numAccesDevices = 0;
        AIOUSB_Init();
        result = AIOUSB_SUCCESS;
        AIODeviceTableAddDeviceToDeviceTableWithUSBDevice( &numAccesDevices, USB_AI16_16E, NULL );
        device = AIODeviceTableGetDeviceAtIndex( numAccesDevices ,  &result );
    }
  
    virtual void TearDown() { 

    }
    int numAccesDevices;
    AIORESULT result;
    AIOUSBDevice *device;
    unsigned short *data;
};

TEST(AIOContinuousBuf,WritingCounts ) 
{
    int num_channels = 16;
    int num_scans = 5000, size = num_scans;
    AIORET_TYPE retval;

    unsigned short *tobuf = (unsigned short *)malloc( num_scans*num_channels*2 );

    AIOContinuousBuf *buf = NewAIOContinuousBufForCounts( 0, num_scans, num_channels );

    for ( int i = 0; i < num_channels*num_scans; i ++ ) tobuf[i] = i;

    retval = buf->PushN( buf, tobuf, num_scans*num_channels );
    EXPECT_EQ( retval, num_scans*num_channels*sizeof(unsigned short) );

    DeleteAIOContinuousBuf(buf);
    free(tobuf);
    
}


TEST(AIOContinuousBuf,CleanupMemory)
{
    int actual_bufsize = 10, buf_unit = 10;
    AIOContinuousBuf *buf = NewAIOContinuousBufTesting( 0, actual_bufsize , buf_unit , AIOUSB_FALSE );
    AIOContinuousBufCreateTmpBuf(buf, 100 );
    DeleteAIOContinuousBuf(buf);
}

class AIOBufParams {
public:
    int num_scans;
    int num_channels ;
    int num_oversamples;
    AIOBufParams( int numscans, int numchannels=16, int numoversamples=0 ) : num_scans(numscans), 
                                                                             num_channels(numchannels), 
                                                                             num_oversamples(numoversamples) { };
    friend std::ostream &operator<<( std::ostream &os, const AIOBufParams &p );
};

std::ostream &operator<< ( std::ostream &os, const AIOBufParams &p ) {
    os << "#Scans=" << p.num_scans << ", #Ch=" << p.num_channels << ", #OSamp=" << p.num_oversamples;
    return os;
}

class AIOContinuousBufThreeParamTest : public ::testing::TestWithParam<AIOBufParams> {};
TEST_P(AIOContinuousBufThreeParamTest,StressTestDrain) 
{
    AIOContinuousBuf *buf;
    int oversamples   = GetParam().num_oversamples;
    int num_scans     = GetParam().num_scans;
    int num_channels  = GetParam().num_channels;
    int repeat_count  = 20;
    int count  =0;
    int retval = 0;
    buf = NewAIOContinuousBufTesting( 0, num_scans , num_channels , AIOUSB_FALSE );
    unsigned short *data = (unsigned short *)malloc(num_scans*(oversamples+1)*num_channels*sizeof(unsigned short) );

    int numAccesDevices = 0;
    AIOUSB_Init();
    AIORESULT result = AIOUSB_SUCCESS;
    AIODeviceTableAddDeviceToDeviceTableWithUSBDevice( &numAccesDevices, USB_AI16_16E, NULL );
    AIOUSBDevice *device = AIODeviceTableGetDeviceAtIndex( numAccesDevices ,  &result );
    AIOUSB_Init();              /* Required */

    AIOContinuousBufInitADCConfigBlock( buf, 20, AD_GAIN_CODE_0_5V, AIOUSB_FALSE , 255, AIOUSB_FALSE );

    EXPECT_EQ( AIOContinuousBufGetOverSample( buf ), 255 );

    free(data);
    DeleteAIOContinuousBuf( buf );
}

// Combine a bunch of different entities
//INSTANTIATE_TEST_CASE_P(AllCombinations, AIOContinuousBufThreeParamTest, ::testing::Combine(::testing::ValuesIn(num_channels),::testing::ValuesIn(num_channels)));
INSTANTIATE_TEST_CASE_P(AllCombinations, AIOContinuousBufThreeParamTest, ::testing::Values( AIOBufParams(1,2,3),
                                                                                          AIOBufParams(100,16,0))
                        );


/**
 * @note originally was stress_copy_counts(bufsize)
 * @details This test checks the stress copying that can go on with the 
 * AIOContinuousBuf buffer. This test verifies the following
 * 1. Reading integer number of scans from the AIOBuffer
 * 2. Correctly not allowing us to read scans into a buffer with storage 
      < scan # of channels for storage
 * 3. Allow reading and writing to occur and loop in the case where
 *    write_pos increases beyond the end of the fifo
 *
 *
 * @brief  We have an AIOContinuousBuf twice the total size of the tobuf.
 *
 *
 * @param num_channels_per_scan := 16
 * @param num_scans             := 2048
 * 
 */
TEST_P(AIOContinuousBufThreeParamTest,BufferScanCounting ) 
{
    unsigned extra = 0;
    int core_size = 256;
    int num_scans     = GetParam().num_scans;
    int num_channels  = GetParam().num_channels;

    int tobuf_size     = num_channels * num_scans * sizeof(unsigned short);
    int use_data_size  = num_channels * num_scans * sizeof(unsigned short );

    unsigned short *use_data  = (unsigned short *)calloc(1, use_data_size  );
    unsigned short *tobuf     = (unsigned short *)calloc(1, tobuf_size );

    AIORET_TYPE retval;
    AIOContinuousBuf *buf = NewAIOContinuousBufForCounts( 0, num_scans+1, num_channels ); 

    /**
     * Pre allocate some values, linear range
     */
    for ( int i = 0 ; i < num_scans*num_channels; i ++ ) { 
        use_data[i] = (unsigned short)i;
    }

    /**
     * @brief We will write slightly less than 1 full buffer ( buffer_size ) of data into the 
     * Aiocontbuf, then set the read position to match the write position, then write one more almost
     * buffer size of data. We should see that the new write position is 2*bytes read % size of the buffer
     */ 
    retval = AIOContinuousBufWriteCounts( buf, use_data, use_data_size, use_data_size, AIOCONTINUOUS_BUF_ALLORNONE );
    EXPECT_EQ( retval, use_data_size ) << "Number of bytes written should equal the full size of the buffer";

    EXPECT_EQ( AIOContinuousBufGetSize(buf)/ sizeof(unsigned short), (1)*num_channels );

    /*----------------------------------------------------------------------------*/
    /**< Cleanup */
    DeleteAIOContinuousBuf(buf); 
    free(use_data);
    free(tobuf);
}

/**
 * @brief Test reading and writing from the AIOBuf
 *
 * @li Try to write in too much and show that it fails
 * @li Try to read out too much
 *
 */
TEST(AIOContinuousBuf,BasicFunctionality ) 
{
    int num_scans = 4000;
    int num_channels = 16;
    int size = num_scans*num_channels;
    AIOContinuousBuf *buf = NewAIOContinuousBuf(0, num_scans  , num_channels );
    int tmpsize = 4*num_scans*num_channels;

    AIOBufferType *frombuf = (AIOBufferType *)malloc(tmpsize*sizeof(AIOBufferType ));
    AIOBufferType *readbuf = (AIOBufferType *)malloc(tmpsize*sizeof(AIOBufferType ));
    AIORET_TYPE retval;
    for ( int i = 0 ; i < tmpsize; i ++ ) { 
        frombuf[i] = rand() % 1000;
    }

    /**
     * Should write since we are writing from a buffer of size tmpsize (80000) into a buffer of 
     * size 4000
     */
    retval = AIOContinuousBufWrite( buf, frombuf , tmpsize, tmpsize*sizeof(AIOBufferType) , AIOCONTINUOUS_BUF_ALLORNONE  );
    EXPECT_EQ( -AIOUSB_ERROR_NOT_ENOUGH_MEMORY, retval ) << "Should have not enough memory error\n";
  
    /* Test writing */
    retval = AIOContinuousBufWrite( buf, frombuf , tmpsize, size*sizeof(AIOBufferType) , AIOCONTINUOUS_BUF_NORMAL  );
    ASSERT_GE( retval, AIOUSB_SUCCESS ) << "not able to write even at write position=" << get_write_pos(buf) << std::endl;
    retval = AIOContinuousBufWrite( buf, frombuf , tmpsize, size*sizeof(AIOBufferType) ,  AIOCONTINUOUS_BUF_NORMAL );
    ASSERT_LT( retval, AIOUSB_SUCCESS ) << "Can't write into a full buffer";

    /* Do a simple reset */
    AIOContinuousBufReset( buf );
    retval = AIOContinuousBufWrite( buf, frombuf , tmpsize, size*sizeof(AIOBufferType) , AIOCONTINUOUS_BUF_NORMAL  );
    ASSERT_GE( retval, AIOUSB_SUCCESS ) << "Able to write to a reset buffer" << get_write_pos(buf) << std::endl;

    /* Full buffer */
    retval = AIOContinuousBufCountScansAvailable( buf );
    EXPECT_EQ( retval, num_scans );
    
    /* Test reading */
    retval = AIOContinuousBufRead( buf, readbuf, num_scans*num_channels*2, num_scans*num_channels*2);
    EXPECT_EQ( retval , num_scans*num_channels*2 );

    /*verify that the data matches the original */
    for ( int i = 0; i < num_scans*num_channels  ; i ++ ) { 
        EXPECT_EQ( frombuf[i], readbuf[i] );
    }

    /*  Testing writing, and then reading the integer number of scans remaining  */
    retval = AIOContinuousBufWrite( buf, frombuf , tmpsize, size*sizeof(AIOBufferType) , AIOCONTINUOUS_BUF_NORMAL  );
    ASSERT_GE( retval, AIOUSB_SUCCESS ) << "Should be able to write to an empty buffer" << get_write_pos(buf) << std::endl;


    int num_scans_to_read = AIOContinuousBufCountScansAvailable( buf );
    retval = AIOContinuousBufReadIntegerScanCounts( buf, readbuf, tmpsize , tmpsize);
    EXPECT_EQ( retval, num_scans );

    DeleteAIOContinuousBuf( buf );

    free(frombuf);
}


#include <unistd.h>
#include <stdio.h>

int main(int argc, char *argv[] )
{
  
  AIORET_TYPE retval;

  testing::InitGoogleTest(&argc, argv);
  testing::TestEventListeners & listeners = testing::UnitTest::GetInstance()->listeners();
#ifdef GTEST_TAP_PRINT_TO_STDOUT
  delete listeners.Release(listeners.default_result_printer());
#endif

  listeners.Append( new tap::TapListener() );
  return RUN_ALL_TESTS();  

}

#endif





