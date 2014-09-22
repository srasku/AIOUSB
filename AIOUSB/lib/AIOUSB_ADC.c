/**
 * @file   AIOUSB_ADC.c
 * @author $Format: %an <%ae>$
 * @date   $Format: %ad$
 * @version $Format: %t$
 * @brief  Configuration functions for ADC elements
 *
 */

#include "AIOUSB_ADC.h"
#include "ADCConfigBlock.h"
#include "AIOUSB_CTR.h"
#include "AIOUSB_Core.h"
#include "AIOTypes.h"
#include "AIODeviceTable.h"
#include "USBDevice.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define BAD_RESULT_AND_GOTO(x, y)  result = x; goto (y);

#ifdef __cplusplus
namespace AIOUSB {
#endif

struct ADRange adRanges[ AD_NUM_GAIN_CODES ] = {
  { 0   , 10 },                 /* AD_GAIN_CODE_0_10V  */
  { -10 , 20 },                 /* AD_GAIN_CODE_10V    */
  { 0   , 5  },                 /* AD_GAIN_CODE_0_5V   */
  { -5  , 10 },                 /* AD_GAIN_CODE_5V     */
  { 0   , 2  },                 /* AD_GAIN_CODE_0_2V   */
  { -2  , 4  },                 /* AD_GAIN_CODE_2V     */
  { 0   , 1  },                 /* AD_GAIN_CODE_0_1V   */
  { -1  , 2  }                  /* AD_GAIN_CODE_1V     */  
};

/* formerly public in the API */
static unsigned long ADC_GetImmediate(
    unsigned long DeviceIndex,
    unsigned long Channel,
    unsigned short *pData);


/*----------------------------------------------------------------------------*/
AIORET_TYPE _check_channels( AIORET_TYPE in, AIOUSBDevice *deviceDesc, int startChannel, int numChannels )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if( !deviceDesc ) {
        return -AIOUSB_ERROR_INVALID_DEVICE;
    } else if ( startChannel < 0 || numChannels < 0 || startChannel + numChannels > ( int )deviceDesc->ADCMUXChannels ) {
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    } else {
        return AIOUSB_SUCCESS;
    }
}
                                       
/*----------------------------------------------------------------------------*/
AIORET_TYPE _check_volts( AIORET_TYPE in, const double volts[] )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( !volts ) {
        return -AIOUSB_ERROR_INVALID_VOLTAGES;
    } else {
        return AIOUSB_SUCCESS;
    }
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE _check_counts( AIORET_TYPE in, const unsigned short counts[] ) 
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( !counts ) {
        return -AIOUSB_ERROR_INVALID_COUNTS;
    } else {
        return AIOUSB_SUCCESS;
    }
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE _check_gain_and_ended( AIORET_TYPE in, unsigned char *pGainCodes, unsigned long bSingleEnded ) 
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if (  pGainCodes == NULL || (bSingleEnded != AIOUSB_FALSE && bSingleEnded != AIOUSB_TRUE )) {
        return -AIOUSB_ERROR_INVALID_PARAMETER; 
    } else {
        return AIOUSB_SUCCESS;
    }
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE _check_valid_adc_channels( AIORET_TYPE in, AIOUSBDevice *deviceDesc ) 
{
   if ( in != AIOUSB_SUCCESS ) {
       return in;
   } else if ( deviceDesc->ADCChannels == 0 ) {
       return -AIOUSB_ERROR_INVALID_DEVICE_CHANNEL_SETTING;
   } else {
       return AIOUSB_SUCCESS;
   }
}

AIORET_TYPE _check_valid_adc_stream( AIORET_TYPE in, AIOUSBDevice *deviceDesc )
{
   if ( in != AIOUSB_SUCCESS ) {
       return in;
   } else if ( deviceDesc->bADCStream == AIOUSB_FALSE ) {
       return -AIOUSB_ERROR_INVALID_DEVICE_STREAM_SETTING;
   } else {
       return AIOUSB_SUCCESS;
   }
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE _check_adc_mux_channels( AIORET_TYPE in, AIOUSBDevice *deviceDesc )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( deviceDesc->ADCMUXChannels == 0 ) {
        return -AIOUSB_ERROR_INVALID_DEVICE_MUX_CHANNEL_SETTING;
    } else {
      return AIOUSB_SUCCESS;
    }
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE _check_range_and_ended_params( AIORET_TYPE in, AIOUSBDevice *deviceDesc , unsigned long ADChannel, unsigned char RangeCode, unsigned long bSingleEnded )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( (RangeCode & ~AD_GAIN_CODE_MASK) != 0 || 
                ( bSingleEnded != AIOUSB_FALSE && bSingleEnded != AIOUSB_TRUE ) || 
                ADChannel >= deviceDesc->ADCMUXChannels ) {
        return -AIOUSB_ERROR_INVALID_DEVICE_SETTING;
    } else {
        return AIOUSB_SUCCESS;
    }
}
AIORET_TYPE _check_start_end_channels( AIORET_TYPE in , AIOUSBDevice *deviceDesc, unsigned long StartChannel, unsigned long EndChannel )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if (EndChannel > deviceDesc->ADCMUXChannels || StartChannel > EndChannel ) {
        return -AIOUSB_ERROR_INVALID_PARAMETER;
    } else {
        return AIOUSB_SUCCESS;
    }
}


/*----------------------------------------------------------------------------*/
AIORET_TYPE _check_valid_trigger( AIORET_TYPE in , unsigned char TriggerMode )
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( (TriggerMode & ~AD_TRIGGER_VALID_MASK) != 0 ) {
        return -AIOUSB_ERROR_INVALID_ADCCONFIG_TRIGGER_SETTING;
    } else {
        return AIOUSB_SUCCESS;
    }
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE _check_cal_mode( AIORET_TYPE in, int CalMode ) 
{
    if ( in != AIOUSB_SUCCESS ) {
        return in;
    } else if ( CalMode != AD_CAL_MODE_NORMAL && 
                CalMode != AD_CAL_MODE_GROUND &&
                CalMode != AD_CAL_MODE_REFERENCE && 
                CalMode != AD_CAL_MODE_BIP_GROUND ) {
        return -AIOUSB_ERROR_INVALID_ADCCONFIG_CAL_SETTING;
    } else {
        return AIOUSB_SUCCESS;
    }
}


/*----------------------------------------------------------------------------*/
unsigned long ADC_ResetDevice( unsigned long DeviceIndex  )
{
    AIORESULT result = AIOUSB_SUCCESS;
    int wValue = 0xe600;
    int wIndex = 1;
    int timeout = 1000;
    unsigned char data[1];
    USBDevice *usb = AIODeviceTableGetUSBDeviceAtIndex( DeviceIndex , &result );
    if ( result != AIOUSB_SUCCESS )
        return result;

    data[0] = 1;
    result = usb->usb_control_transfer(usb,
                                       USB_WRITE_TO_DEVICE,
                                       0xa0,
                                       wValue,
                                       wIndex,
                                       data,
                                       1,
                                       timeout
                                       );
    if( result <= AIOUSB_SUCCESS )
        goto out_ADC_ResetDevice;
    
    data[0] = 0;
    sleep(2);
    result = usb->usb_control_transfer(usb,
                                       USB_WRITE_TO_DEVICE,
                                       0xa0,
                                       wValue,
                                       wIndex,
                                       data,
                                       1,
                                       timeout
                                       );
 out_ADC_ResetDevice:
    AIOUSB_UnLock();
    return result;
}



/*----------------------------------------------------------------------------*/
void ADC_SetTestingMode(ADCConfigBlock *config, AIOUSB_BOOL testing )
{
  assert(config);
  config->testing = testing;
}

/*----------------------------------------------------------------------------*/
AIOUSB_BOOL ADC_GetTestingMode(ADCConfigBlock *config, AIOUSB_BOOL testing )
{
  assert(config);
  return config->testing;
}

/*----------------------------------------------------------------------------*/
/* AIORET_TYPE ADC_WriteADCConfigBlock( unsigned long DeviceIndex , ADCConfigBlock *config ) */
/* { */
/*     unsigned long result; */
/*     AIORET_TYPE retval; */
/*     DeviceDescriptor *deviceDesc = AIOUSB_GetDevice_Lock( DeviceIndex, &result ); */
/*     if ( !deviceDesc || result != AIOUSB_SUCCESS )  { */
/*         retval = -result; */
/*         goto out_ADC_WriteADCConfigBlock; */
/*     } */
/*     result = GenericVendorWrite( DeviceIndex ,  */
/*                                  AUR_ADC_SET_CONFIG, */
/*                                  0, */
/*                                  0,  */
/*                                  config->registers, */
/*                                  config->size */
/*                                  ); */
/*     retval = ( result  == AIOUSB_SUCCESS ? AIOUSB_SUCCESS : - result ); */
/* out_ADC_WriteADCConfigBlock: */
/*     AIOUSB_UnLock(); */
/*     return retval; */
/* } */

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADC_ReadADCConfigBlock( unsigned long DeviceIndex , ADCConfigBlock *config )
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIORET_TYPE retval;
    AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ) {
        retval = -result;
        goto out_ADC_ReadADConfigBlock;
    }
    
    /* Check size ...not necessary */
    result = GenericVendorRead( DeviceIndex, 
                                AUR_ADC_GET_CONFIG , 
                                0 , 
                                0, 
                                config->registers,
                                &config->size
                                );
    if( result != AIOUSB_SUCCESS ) 
        retval = -result ;
    else
        retval = AIOUSB_SUCCESS;
    
    
out_ADC_ReadADConfigBlock:
    AIOUSB_UnLock();
    return retval;

}

/*----------------------------------------------------------------------------*/
/**
 * @brief
 * @param DeviceIndex
 * @param forceRead
 */
AIORET_TYPE ReadConfigBlock( USBDevice *usb, ADCConfigBlock *config )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    ADCConfigBlock configBlock;
    int bytesTransferred;
    assert(usb != NULL && config != NULL );
    
    if ( !usb ) 
        return AIOUSB_ERROR_INVALID_USBDEVICE;
    if ( !config )
        return AIOUSB_ERROR_INVALID_ADCCONFIG;

    ADCConfigBlockInitialize( &configBlock, config->device );
    
    AIOUSB_UnLock();
    if( configBlock.testing != AIOUSB_TRUE ) {
            bytesTransferred = usb->usb_control_transfer(usb,
                                                         USB_READ_FROM_DEVICE,
                                                         AUR_ADC_GET_CONFIG,
                                                         0,
                                                         0,
                                                         configBlock.registers,
                                                         configBlock.size,
                                                         configBlock.timeout
                                                         );
            
            if ( bytesTransferred != ( int ) configBlock.size) {
                retval = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
                goto out_ReadConfigBlock;
            }
            /*
             * check and correct settings read from device
             */
            ADC_VerifyAndCorrectConfigBlock( &configBlock , config->device );

            /* deviceDesc->cachedConfigBlock = configBlock; */
            retval = ADCConfigBlockCopy( config, &configBlock );
        }
    /* } */
 out_ReadConfigBlock:
    AIOUSB_UnLock();

    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 */
AIORET_TYPE WriteConfigBlock( USBDevice *usb, ADCConfigBlock *config )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    assert(usb != NULL && config != NULL ) ;

    if ( !usb ) {
        retval = -AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
        goto out_WriteConfigBlock;
    }
    if ( !config ) {
        retval =  -AIOUSB_ERROR_INVALID_ADCCONFIG;
        goto out_WriteConfigBlock;
    }
    if ( config->size > AD_MAX_CONFIG_REGISTERS || config->size < AD_MIN_CONFIG_REGISTERS ) { 
        retval =  -AIOUSB_ERROR_INVALID_ADCCONFIG_SETTING; 
        goto out_WriteConfigBlock;
    }
    if ( config->timeout > AD_MAX_TIMEOUT || config->timeout < AD_MIN_TIMEOUT ) {
        retval = -AIOUSB_ERROR_INVALID_TIMEOUT;
        goto out_WriteConfigBlock;
    }

    if ( config->testing != AIOUSB_TRUE ) {
        int bytesTransferred = usb->usb_control_transfer(usb,
                                                         USB_WRITE_TO_DEVICE, 
                                                         AUR_ADC_SET_CONFIG,
                                                         0, 
                                                         0, 
                                                         config->registers, 
                                                         config->size, 
                                                         config->timeout
                                                       );
        if ( bytesTransferred != ( int )config->size )
            retval = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
    }

 out_WriteConfigBlock:
    AIOUSB_UnLock();
    return retval;
}

/*------------------------------------------------------------------------*/
/**
 * @brief Performs a number of ADC_GetImmediate calls and then averages out the values
 *       to determine adequate values for the Ground and Reference values
 * @param DeviceIndex
 * @param grounCounts
 * @param referenceCounts
 */
AIORESULT ADC_Acquire_Reference_Counts(
                                       unsigned long DeviceIndex,
                                       double *groundCounts,
                                       double *referenceCounts
                                       )
{
    int reading;
    double averageCounts;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }
    USBDevice *usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    if ( !usb ) {
        AIOUSB_UnLock();
        return AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
    }

    for(reading = 0; reading <= 1; reading++) {
          AIOUSB_Lock();
          /* ADCConfigBlockSetCalMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), (reading == 0) ? AD_CAL_MODE_GROUND : AD_CAL_MODE_REFERENCE); */
          ADCConfigBlockSetCalMode( AIOUSBDeviceGetADCConfigBlock( deviceDesc ), (reading == 0) ? AD_CAL_MODE_GROUND : AD_CAL_MODE_REFERENCE);

          result = WriteConfigBlock( usb,  AIOUSBDeviceGetADCConfigBlock( deviceDesc ) );

          if (result == AIOUSB_SUCCESS) {
            /*
             * average a bunch of readings to get a nice, stable
             * reading
             */
                int AVERAGE_SAMPLES = 256;
                unsigned MAX_GROUND = 0x00ff, MIN_REFERENCE = 0xf000;
                long countsSum = 0;
                int sample;
                unsigned short counts[ MAX_IMM_ADCS ];
                for(sample = 0; sample < AVERAGE_SAMPLES; sample++) {
                      result = ADC_GetImmediate(DeviceIndex, 0, counts);
                      if(result == AIOUSB_SUCCESS)
                          countsSum += counts[ 0 ];
                      else
                          goto RETURN_AIOUSB_GetBulkAcquire;
                  }

                averageCounts = countsSum / ( double )AVERAGE_SAMPLES;

                if(reading == 0) {
                      if(averageCounts <= MAX_GROUND)
                          *groundCounts = averageCounts;
                      else{
                            result = AIOUSB_ERROR_INVALID_DATA;
                            goto RETURN_AIOUSB_GetBulkAcquire;
                        }
                  }else {
                      if(
                          averageCounts >= MIN_REFERENCE &&
                          averageCounts <= AI_16_MAX_COUNTS
                          )
                          *referenceCounts = averageCounts;
                      else{
                            result = AIOUSB_ERROR_INVALID_DATA;
                            goto RETURN_AIOUSB_GetBulkAcquire;
                        }
                  }
            }
      }
RETURN_AIOUSB_GetBulkAcquire:
    return result;
}



/**
 * @brief Performs a scan and averages the voltage values.
 * @param DeviceIndex
 * @param counts
 * @return
 */
PRIVATE AIORESULT AIOUSB_GetScan(
                                 unsigned long DeviceIndex,
                                 unsigned short counts[]
                                 )
{
    if(counts == NULL)
        return AIOUSB_ERROR_INVALID_PARAMETER;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    USBDevice *usb;
    int libusbResult;
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    if (deviceDesc->bADCStream == AIOUSB_FALSE) {
        result = AIOUSB_ERROR_NOT_SUPPORTED;
        return result;
    }
    usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    if (!usb )
        return AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
    
    result = ReadConfigBlock( usb,  AIOUSBDeviceGetADCConfigBlock( deviceDesc )  );

    if(result != AIOUSB_SUCCESS) {
        AIOUSB_UnLock();
        return result;
    }
    ADCConfigBlock *origConfigBlock  = AIOUSBDeviceGetADCConfigBlock( deviceDesc );
    AIOUSB_BOOL configChanged        = AIOUSB_FALSE;
    AIOUSB_BOOL discardFirstSample   = deviceDesc->discardFirstSample;
    int startChannel                 = ADCConfigBlockGetStartChannel( origConfigBlock );
    int endChannel                   = ADCConfigBlockGetEndChannel( origConfigBlock );

    if ( startChannel < 0 || endChannel < 0 ) {
        return AIOUSB_ERROR_INVALID_DATA;
    }
    int overSample                   = ADCConfigBlockGetOversample( origConfigBlock  );
    
    if ( overSample < 0 ) 
        return AIOUSB_ERROR_INVALID_ADCCONFIG_OVERSAMPLE_SETTING;

    int numChannels = endChannel - startChannel + 1;
    
    /**
     * in theory, all the A/D functions, including
     * AIOUSB_GetScan(), should work in all measurement
     * modes, including calibration mode; in practice,
     * however, the device will return only a single
     * sample in calibration mode; therefore, users must
     * be careful to select a single channel and set
     * oversample to zero during calibration mode;
     * attempting to read more than one channel or use an
     * oversample setting of more than zero in calibration
     * mode will result in a timeout error; as a
     * convenience to the user we automatically impose
     * this restriction here in AIOUSB_GetScan(); if the
     * device is changed to permit normal use of the A/D
     * functions in calibration mode, we will have to
     * modify this function to somehow recognize which
     * devices support that capability, or simply delete
     * this restriction altogether and rely on the users'
     * good judgment
     */

    int calMode = ADCConfigBlockGetCalMode( origConfigBlock );
    if(calMode == AD_CAL_MODE_GROUND || calMode == AD_CAL_MODE_REFERENCE) {
        if (numChannels > 1) {
            ADCConfigBlockSetScanRange( origConfigBlock, startChannel, endChannel = startChannel);
            numChannels = 1;
            configChanged = AIOUSB_TRUE;
        }
        if(overSample > 0) {
            ADCConfigBlockSetOversample( origConfigBlock, overSample = 0);
            configChanged = AIOUSB_TRUE;
        }
        discardFirstSample = AIOUSB_FALSE;           // this feature can't be used in calibration mode either
    }

    /**
     * turn scan on and turn timer and external trigger
     * off
     */
    int origTriggerMode = ADCConfigBlockGetTriggerMode( origConfigBlock );
    int triggerMode = origTriggerMode;
    triggerMode |= AD_TRIGGER_SCAN;                                              // enable scan
    triggerMode &= ~(AD_TRIGGER_TIMER | AD_TRIGGER_EXTERNAL);         // disable timer and external trigger
    if (triggerMode != origTriggerMode) {
        ADCConfigBlockSetTriggerMode( origConfigBlock, triggerMode );
        configChanged = AIOUSB_TRUE;
    }

    /**
     * the oversample setting dictates how many samples to
     * take _in addition_ to the primary sample; if
     * oversample is zero, we take just one sample for
     * each channel; if oversample is greater than zero
     * then we average the primary sample and all of its
     * over-samples; if the discardFirstSample setting is
     * enabled, then we discard the primary sample,
     * leaving just the over-samples; thus, if
     * discardFirstSample is enabled, we must take at
     * least one over-sample in order to have any data
     * left; there's another complication: the device
     * buffer is limited to a small number of samples, so
     * we have to limit the number of over-samples to what
     * the device buffer can accommodate, so the actual
     * oversample setting depends on the number of
     * channels being scanned; we also preserve and
     * restore the original oversample setting specified
     * by the user; since the user is expecting to average
     * (1 + oversample) samples, then if
     * discardFirstSample is enabled we simply always add
     * one
     */

    int origOverSample = overSample;
    int samplesPerChannel = 1 + origOverSample;
    if(discardFirstSample)
        samplesPerChannel++;
    if(samplesPerChannel > 256)
        samplesPerChannel = 256;              // rained by maximum oversample of 255

    /**
     * make sure device buffer can accommodate this number
     * of samples
     */
    int DEVICE_SAMPLE_BUFFER_SIZE = 1024;   /* number of samples device can buffer */
    if ((numChannels * samplesPerChannel) > DEVICE_SAMPLE_BUFFER_SIZE)
        samplesPerChannel = DEVICE_SAMPLE_BUFFER_SIZE / numChannels;

    overSample = samplesPerChannel - 1;

    if (overSample != origOverSample) {
        ADCConfigBlockSetOversample( origConfigBlock, overSample);
        configChanged = AIOUSB_TRUE;
    }

    if (configChanged) {

        result = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
        AIOUSB_Lock();
    }
    if (result != AIOUSB_SUCCESS)
        return result;

    int numSamples = numChannels * samplesPerChannel;
    unsigned short numSamplesHigh = ( unsigned short )(numSamples >> 16);
    unsigned short numSamplesLow = ( unsigned short )numSamples;
    int numBytes = numSamples * sizeof(unsigned short);
    unsigned short * sampleBuffer = ( unsigned short* )malloc(numBytes);
    /* assert(sampleBuffer != 0); */
    if (sampleBuffer != 0) {
        AIOUSB_UnLock();
        int bytesTransferred = usb->usb_control_transfer(usb,
                                                         USB_WRITE_TO_DEVICE, 
                                                         AUR_START_ACQUIRING_BLOCK,
                                                         numSamplesHigh, 
                                                         numSamplesLow, 
                                                         0, 
                                                         0, 
                                                         deviceDesc->commTimeout
                                                         );
        if(bytesTransferred == 0) {

            /* request AUR_ADC_IMMEDIATE triggers the sampling of data */

            bytesTransferred = usb->usb_control_transfer(usb,
                                                         USB_READ_FROM_DEVICE, 
                                                         AUR_ADC_IMMEDIATE,
                                                         0, 
                                                         0, 
                                                         ( unsigned char* )sampleBuffer, 
                                                         sizeof(unsigned short),
                                                         deviceDesc->commTimeout
                                                         );

            if (bytesTransferred == sizeof(unsigned short) ) {
                /* int libusbResult = AIOUSB_BulkTransfer(deviceHandle, */
                /*                                        LIBUSB_ENDPOINT_IN | USB_BULK_READ_ENDPOINT, */
                /*                                        ( unsigned char* )sampleBuffer,  */
                /*                                        numBytes,  */
                /*                                        &bytesTransferred, */
                /*                                        deviceDesc->commTimeout */
                /*                                        ); */
                libusbResult = usb->usb_bulk_transfer( usb,
                                                       LIBUSB_ENDPOINT_IN | USB_BULK_READ_ENDPOINT,
                                                       ( unsigned char* )sampleBuffer,
                                                       numBytes,
                                                       &bytesTransferred,
                                                       deviceDesc->commTimeout
                                                       );


                if (libusbResult != LIBUSB_SUCCESS) {
                    result = LIBUSB_RESULT_TO_AIOUSB_RESULT(libusbResult);
                } else if(bytesTransferred != numBytes) {
                    result = AIOUSB_ERROR_INVALID_DATA;
                } else {
                    /**
                     * Compute the average of all the samples taken
                     * for each channel, discarding the first sample
                     * if that option is enabled; each byte in
                     * sampleBuffer[] is 1 of 2 bytes for each sample,
                     * the first byte being the LSB and the second
                     * byte the MSB, in other words, little-endian
                     * format; so for convenience we simply declare
                     * sampleBuffer[] to be of type 'unsigned short'
                     * and the data is already in the correct format;
                     * the device returns data only for the channels
                     * requested, from startChannel to endChannel;
                     * AIOUSB_GetScan() returns the averaged data
                     * readings in counts[], putting the reading for
                     * startChannel in counts[0], and the reading for
                     * endChannel in counts[numChannels-1]
                     */

                    int samplesToAverage = discardFirstSample? samplesPerChannel - 1: samplesPerChannel;
                    int sampleIndex = 0;
                    int channel;
                    for(channel = 0; channel < numChannels; channel++) {
                        unsigned long sampleSum = 0;
                        if(discardFirstSample)
                            sampleIndex++;                          // skip over first sample
                        int sample;
                        for(sample = 0; sample < samplesToAverage; sample++)
                            sampleSum += sampleBuffer[ sampleIndex++ ];
                        counts[ channel ] = ( unsigned short )
                            ((sampleSum + samplesToAverage / 2) / samplesToAverage);
                    }
                }
            }else
                result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
        }else
            result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
        free(sampleBuffer);
    } else {
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
        AIOUSB_UnLock();
    }

    if (configChanged) {
        AIOUSB_Lock();
        /* deviceDesc->cachedConfigBlock = origConfigBlock; */
        AIOUSB_UnLock();                // unlock while communicating with device
        /* WriteConfigBlock(DeviceIndex); */
        result = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
    }

    AIOUSB_UnLock();
    return result;
}

unsigned ADC_GetOversample_Cached( ADCConfigBlock *config )
{
  assert(config);
  return config->registers[ AD_CONFIG_OVERSAMPLE ];
}

unsigned ADC_GainCode_Cached( ADCConfigBlock *config, unsigned channel)
{
  assert(config);
  AIOUSBDevice *deviceDesc = (AIOUSBDevice *)config->device;
  unsigned gainCode = (config->registers[ AD_CONFIG_GAIN_CODE + channel / deviceDesc->ADCChannelsPerGroup ] & ( unsigned char )AD_GAIN_CODE_MASK );
  return gainCode;
}

AIOUSBDevice *AIOUSB_GetDevice_NoCheck( unsigned long DeviceIndex  )
{
    return &deviceTable[DeviceIndex];
}

/**
 * @brief Combines the oversample channels as well as combines the rules for removing
 *       the first discard channel if it is enabled. Channels are average and then 
 *       the resulting array size is altered to reflect the new size of the counts
 *       that has been reduced by replacing all oversamples of each channel
 *       with the average value.
 * @param DeviceIndex 
 * @param counts 
 * @param size 
 * @return 
 */
AIORET_TYPE cull_and_average_counts( unsigned long DeviceIndex, 
                                                   unsigned short *counts,
                                                   unsigned *size ,
                                                   unsigned numChannels
                                                   )
{
    unsigned pos, cur;
    if(counts == NULL)
        return (AIORET_TYPE)-AIOUSB_ERROR_INVALID_PARAMETER;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }
    AIOUSB_UnLock();

    AIOUSB_BOOL discardFirstSample  = deviceDesc->discardFirstSample;
    unsigned numOverSamples         = ADC_GetOversample_Cached( AIOUSBDeviceGetADCConfigBlock( deviceDesc ) );
    unsigned long sum;
    int repeat = 0;
    for ( cur = 0, pos = 0; cur < *size ; ) {
        for ( unsigned channel = 0; channel < numChannels && cur < *size; channel ++ , pos ++) {
            sum = 0;
            /* Needs bail out when cur > *size */
            for( unsigned os = 0; os <= numOverSamples && cur < *size; os ++ , cur ++ ) {
              /* printf("Pos=%d, Cur=%d, Ch=%d, Os=%d\n", pos, cur, channel,os); */
                if ( discardFirstSample && os == 0 ) {
                } else {
                    sum += counts[cur];
                }
            }
            if( discardFirstSample ) { 
              if( numOverSamples ) {
                sum = (sum / numOverSamples);                
              } else {
                sum = ( sum / (numOverSamples + 1));
              }
            } else {
              sum = ( sum / (numOverSamples + 1));
            }
            counts[pos] = (unsigned short)sum;
        }
        repeat ++;
    }
    *size = pos;
    return (AIORET_TYPE)pos;
}

/**
 * @brief
 * @param DeviceIndex
 * @param startChannel
 * @param numChannels
 * @param counts
 * @param volts
 * @return
 */
PRIVATE AIORET_TYPE  AIOUSB_ArrayCountsToVolts(
                                               unsigned long DeviceIndex,
                                               int startChannel,
                                               int numChannels,
                                               const unsigned short counts[],
                                               double volts[]
                                               )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT*)&retval );
    USBDevice *usb;
    struct ADRange * range;
    int gainCode;

    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_channels(_check_counts( _check_volts( retval, volts), counts ),deviceDesc,startChannel,numChannels),
                             usb,  
                             out_AIOUSB_ArrayCountsToVolts
                             );

     retval = ReadConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc )  );

     if (retval == AIOUSB_SUCCESS) {
          AIOUSB_Lock();
          for(int channel = 0; channel < numChannels; channel++) {
               gainCode = ADCConfigBlockGetGainCode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), startChannel + channel);
               range = &adRanges[ gainCode ];
               volts[ channel ] = ( (( double )counts[ channel ] / ( double )AI_16_MAX_COUNTS) * range->range ) + range->minVolts;
          }
     }
 out_AIOUSB_ArrayCountsToVolts:
     AIOUSB_UnLock();
     return retval;
}

/* if ( retval != AIOUSB_SUCCESS ) { */
/*     return -retval; */
/* } */
/* if ( !usb  )  */
/*     return AIOUSB_ERROR_USBDEVICE_NOT_FOUND; */
/* if( startChannel < 0 || */
/*     numChannels < 0 || */
/*     startChannel + numChannels > ( int )deviceDesc->ADCMUXChannels || */
/*     counts == NULL || */
/*     volts == NULL */
/*     ) { */
/*     AIOUSB_UnLock(); */
/*     return AIOUSB_ERROR_INVALID_PARAMETER; */
/*  } */

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param startChannel
 * @param numChannels
 * @param volts
 * @param counts
 * @return
 */
PRIVATE AIORET_TYPE AIOUSB_ArrayVoltsToCounts( unsigned long DeviceIndex,
                                              int startChannel,
                                              int numChannels,
                                              const double volts[],
                                              unsigned short counts[]
                                              )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT *)&retval );
    USBDevice *usb;
    int channel , gainCode, rawCounts;
    struct ADRange * range;
    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_mutex(_check_channels(_check_counts( _check_volts( retval, volts), counts ),deviceDesc,startChannel,numChannels)),
                             usb,  
                             out_AIOUSB_ArrayVoltsToCounts 
                             );

    retval = ReadConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc )  );
    if ( retval != AIOUSB_SUCCESS )
        goto out_AIOUSB_ArrayVoltsToCounts;

    for(channel = 0; channel < numChannels; channel++) {
        gainCode = ADCConfigBlockGetGainCode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), startChannel + channel);
        range = &adRanges[ gainCode ];
        rawCounts = round( ( double )AI_16_MAX_COUNTS * (volts[ channel ] - range->minVolts) / range->range );
        if(rawCounts < 0)
            rawCounts = 0;
        else if(rawCounts > AI_16_MAX_COUNTS)
            rawCounts = AI_16_MAX_COUNTS;
        counts[ channel ] = ( unsigned short )rawCounts;
    }

 out_AIOUSB_ArrayVoltsToCounts:
    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param ChannelIndex
 * @param pBuf
 * @return
 */
AIORET_TYPE ADC_GetChannelV(
                              unsigned long DeviceIndex,
                              unsigned long ChannelIndex,
                              double *pBuf
                              )
{
    if(!AIOUSB_Lock())
        return AIOUSB_ERROR_INVALID_MUTEX;
    
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    USBDevice *usb;
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }
    usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    if (!usb )
        return AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
    

    if(deviceDesc->bADCStream == AIOUSB_FALSE) {
          AIOUSB_UnLock();
          return AIOUSB_ERROR_NOT_SUPPORTED;
      }
    if(
        pBuf == NULL ||
        ChannelIndex >= deviceDesc->ADCMUXChannels
        ) {
          AIOUSB_UnLock();
          return AIOUSB_ERROR_INVALID_PARAMETER;
      }

    /**
     * there is no guarantee that ChannelIndex, passed by the user, is
     * within the current channel scan range; if it is not, then valid
     * data cannot be returned; in addition, since we're only returning
     * the data for a single channel, there's no need to scan all the
     * channels; the Windows implementation attempts to improve
     * performance by caching all the values read; but the technique is
     * riddled with problems; first of all, it can easily return extremely
     * stale data, without any indication to the user; secondly, it can
     * return data for channels that weren't even scanned, without any
     * indication to the user; thirdly, caching is unnecessary; if the
     * user wants to read a single channel they can call
     * ADC_GetChannelV(); if the user wants to improve performance by
     * reading multiple channels they can call ADC_GetScanV(); so to
     * address all these issues, we temporarily compress the scan range to
     * just ChannelIndex and then restore it when we're done; so in this
     * implementation all calls to ADC_GetChannelV() return "real-time"
     * data for the specified channel
     */
    AIOUSB_UnLock();                                        // unlock while communicating with device

    result = ReadConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc )  );

    if(result == AIOUSB_SUCCESS) {
          AIOUSB_Lock();
          ADCConfigBlock *origConfigBlock = AIOUSBDeviceGetADCConfigBlock( deviceDesc );
          ADCConfigBlockSetScanRange( origConfigBlock, ChannelIndex, ChannelIndex);
          AIOUSB_UnLock();                              // unlock while communicating with device
          /* result = WriteConfigBlock(DeviceIndex); */
          result = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));                    
          unsigned short counts;
          result = AIOUSB_GetScan(DeviceIndex, &counts);
          if(result == AIOUSB_SUCCESS) {
                double volts;
                result = AIOUSB_ArrayCountsToVolts(DeviceIndex, ChannelIndex, 1, &counts, &volts);
                if(result == AIOUSB_SUCCESS)
                    *pBuf = volts;
                else
                    *pBuf = 0.0;
            }
          AIOUSB_Lock();
          deviceDesc->cachedConfigBlock = *origConfigBlock;
          AIOUSB_UnLock();                              // unlock while communicating with device
          /* WriteConfigBlock(DeviceIndex); */
          /* result = WriteConfigBlock( deviceDesc, AIOUSBDeviceGetADCConfigBlock( deviceDesc )); */
          result = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
      }

    return result;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief Preferred way to get immediate scan readings. Will Scan all channels ( ie vectored ) 
 *       perform averaging and culling of data. 
 * @param DeviceIndex
 * @param pBuf
 * @return
 */
AIORET_TYPE ADC_GetScanV(
                           unsigned long DeviceIndex,
                           double *pBuf
                           )
{
    int startChannel, endChannel;
    unsigned channel;
    AIORESULT result = AIOUSB_SUCCESS;
    unsigned short *counts = NULL;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }
    if (pBuf == NULL)
        return AIOUSB_ERROR_INVALID_PARAMETER;

    if (!AIOUSB_Lock())
        return AIOUSB_ERROR_INVALID_MUTEX;

    if (!deviceDesc->bADCStream) {
        result = AIOUSB_ERROR_NOT_SUPPORTED;
        goto out_ADC_GetScanV;
    }
    /**
     * get raw A/D counts
     */
    counts = ( unsigned short* )malloc(deviceDesc->ADCMUXChannels * sizeof(unsigned short));

    if (!counts ) {
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
        goto out_ADC_GetScanV;
    }
    
    AIOUSB_UnLock();            // unlock while communicating with device
    result = ADC_GetScan(DeviceIndex, counts);
    AIOUSB_Lock();
    if (result == AIOUSB_SUCCESS) {
        /**
         * Convert from A/D counts to volts; only
         * the channels from startChannel to
         * endChannel contain valid data, so we
         * only convert those; pBuf[] is expected
         * to contain entries for all the A/D
         * channels; so for cleanliness, we zero
         * out the channels in pBuf[] that aren't
         * going to be filled in with real readings
         */
        startChannel = ADCConfigBlockGetStartChannel(AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
        endChannel   = ADCConfigBlockGetEndChannel(AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
        if ( startChannel < 0 || endChannel < 0 || startChannel > endChannel ) {
            return AIOUSB_ERROR_INVALID_DATA;
        }

        /**
         * zero out unused channels
         */
        for(channel = 0; channel < deviceDesc->ADCMUXChannels; channel++) {
            if(channel < (unsigned)startChannel || channel > (unsigned)endChannel )
                pBuf[ channel ] = 0.0;
        }

        /**
         * convert remaining channels to volts
         */
        result = AIOUSB_ArrayCountsToVolts(DeviceIndex, startChannel, endChannel - startChannel + 1,
                                           counts + startChannel, pBuf + startChannel);
    }

    free(counts);
 out_ADC_GetScanV:
    AIOUSB_UnLock();
    return result;
}

/**
 * @param DeviceIndex
 * @param pBuf
 * @return
 */
AIORET_TYPE ADC_GetScan(
                          unsigned long DeviceIndex,
                          unsigned short *pBuf
                          )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT *)&retval );
    int startChannel;

    if ( retval != AIOUSB_SUCCESS )
        goto out_ADC_GetScan;

    if(pBuf == NULL) {
        retval = -AIOUSB_ERROR_INVALID_PARAMETER;
        goto out_ADC_GetScan;
    }

    if(!AIOUSB_Lock()) {
        retval = AIOUSB_ERROR_INVALID_MUTEX;
        goto out_ADC_GetScan;
    }

    if(deviceDesc->bADCStream == AIOUSB_FALSE) {
        retval = AIOUSB_ERROR_NOT_SUPPORTED;
        goto out_ADC_GetScan;
    }

    /**
     * pBuf[] is expected to contain entries for all the A/D channels,
     * even though we may be reading only a few channels; so for
     * cleanliness, we zero out the channels in pBuf[] that aren't
     * going to be filled in with real readings
     */
    memset(pBuf, 0, deviceDesc->ADCMUXChannels * sizeof(unsigned short));
    startChannel = ADCConfigBlockGetStartChannel(AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
    if ( startChannel < 0 ) {
        retval = AIOUSB_ERROR_INVALID_ADCCONFIG_CHANNEL_SETTING;
        goto out_ADC_GetScan;
    }

 out_ADC_GetScan:
    AIOUSB_UnLock();
    return AIOUSB_GetScan(DeviceIndex, pBuf + startChannel);
}


/**
 * @brief Copies the old Cached Config block registers into the pConfigBuf
 *       object.
 * @param DeviceIndex
 * @param pConfigBuf
 * @param ConfigBufSize
 * @return
 */
AIORET_TYPE ADC_GetConfig(
                            unsigned long DeviceIndex,
                            unsigned char *ConfigBuf,
                            unsigned long *ConfigBufSize
                            )
{
    AIORESULT result = AIOUSB_SUCCESS;
    if( ConfigBuf == NULL || ConfigBufSize == NULL ) 
         return AIOUSB_ERROR_INVALID_PARAMETER;

    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS )
        return result;
    USBDevice *usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    if (!usb )
        return AIOUSB_ERROR_USBDEVICE_NOT_FOUND;

    if(*ConfigBufSize < deviceDesc->ConfigBytes) {
        *ConfigBufSize = deviceDesc->ConfigBytes;
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
        goto out_ADC_GetConfig;
    }

    AIOUSB_UnLock();                                        // unlock while communicating with device
    result = ReadConfigBlock( usb,  AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
    if(result == AIOUSB_SUCCESS) {
          assert(deviceDesc->cachedConfigBlock.size > 0 &&
                 deviceDesc->cachedConfigBlock.size <= AD_MAX_CONFIG_REGISTERS);
          AIOUSB_Lock();
          memcpy(ConfigBuf, deviceDesc->cachedConfigBlock.registers, deviceDesc->cachedConfigBlock.size);
          *ConfigBufSize = deviceDesc->cachedConfigBlock.size;
          AIOUSB_UnLock();
      }

out_ADC_GetConfig:
    AIOUSB_UnLock();
    return result;
}

int adcblock_valid_trigger_settings(ADCConfigBlock *config )
{
     return (config->registers[ AD_CONFIG_TRIG_COUNT ] & ~AD_TRIGGER_VALID_MASK ) == 0;
}



AIORET_TYPE valid_config_block( ADCConfigBlock *config )
{
     unsigned long result = 0;
     return result;
}

int adcblock_valid_size( ADCConfigBlock *config )
{
     return config->size > 0 && config->size <= AD_MAX_CONFIG_REGISTERS;
}

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param pConfigBuf
 * @param ConfigBufSize
 * @return
 */
AIORET_TYPE ADC_SetConfig(
                            unsigned long DeviceIndex,
                            unsigned char *pConfigBuf,
                            unsigned long *ConfigBufSize
                            )
{
    AIORESULT retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &retval );
    ADCConfigBlock configBlock = {0};
    USBDevice *usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    int endChannel;

    if ( retval != AIOUSB_SUCCESS ) {
        AIOUSB_UnLock();
        return retval;
    }

    /* usb = AIOUSBDeviceGetUSBHandle( deviceDesc ); */
    /* if (!usb ) { */
    /*     retval = -AIOUSB_ERROR_USBDEVICE_NOT_FOUND; */
    /*     goto out_ADC_SetConfig; */
    /* } */

    /* if( pConfigBuf == NULL || ConfigBufSize == NULL ) { */
    /*     retval = -AIOUSB_ERROR_INVALID_PARAMETER; */
    /*     goto out_ADC_SetConfig; */
    /* } */

    /* if(*ConfigBufSize < deviceDesc->ConfigBytes) { */
    /*     *ConfigBufSize = deviceDesc->ConfigBytes; */
    /*     retval = -AIOUSB_ERROR_INVALID_PARAMETER; */
    /*     goto out_ADC_SetConfig; */
    /* } */
    
    configBlock.size = *ConfigBufSize;
    configBlock.timeout = deviceDesc->commTimeout;
    
    memcpy(configBlock.registers, pConfigBuf, configBlock.size);
    
    /* if( ! adcblock_valid_size( &configBlock ) ) { */
    /*     retval = -AIOUSB_ERROR_INVALID_ADCCONFIG_CHANNEL_SETTING; */
    /*     goto out_ADC_SetConfig; */
    /* } */

    /* if( !adcblock_valid_channel_settings( &configBlock , deviceDesc->ADCMUXChannels )  ) { */
    /*     retval = -AIOUSB_ERROR_INVALID_ADCCONFIG_CHANNEL_SETTING; */
    /*     goto out_ADC_SetConfig; */
    /* } */

    _adcblock_valid_channel_settings( retval, &configBlock , deviceDesc->ADCMUXChannels );

    if( !VALID_ENUM( ADCalMode , configBlock.registers[ AD_CONFIG_CAL_MODE ] ) ) {
        retval = -AIOUSB_ERROR_INVALID_ADCCONFIG_CAL_SETTING;
        goto out_ADC_SetConfig;
    }

    /* if( !adcblock_valid_trigger_settings( &configBlock ) )  { */
    /*     retval = -AIOUSB_ERROR_INVALID_ADCCONFIG_SETTING; */
    /*     goto out_ADC_SetConfig;   */
    /* } */

    endChannel = ADCConfigBlockGetEndChannel( &configBlock );

    if( endChannel >= (int)deviceDesc->ADCMUXChannels || ADCConfigBlockGetStartChannel(&configBlock) > endChannel ) {
        retval = -AIOUSB_ERROR_INVALID_PARAMETER;
        goto out_ADC_SetConfig;
    }

    retval = WriteConfigBlock( usb, &configBlock );    

    if(retval == AIOUSB_SUCCESS)
        *ConfigBufSize = configBlock.size;
    
out_ADC_SetConfig:
     AIOUSB_UnLock();
     return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param pGainCodes
 * @param bSingleEnded
 * @return
 */
AIORET_TYPE ADC_RangeAll(
                           unsigned long DeviceIndex,
                           unsigned char *pGainCodes,
                           unsigned long bSingleEnded
                           )
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIORESULT retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT*)&retval );
    USBDevice *usb;
    unsigned channel;

    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_mutex(_check_gain_and_ended( _check_valid_adc_channels( retval,  deviceDesc ), pGainCodes, bSingleEnded )),
                             usb,  
                             out_ADC_RangeAll
                             );    
    /*
     * validate gain codes; they should be just gain codes; single-ended or differential
     * mode is specified by bSingleEnded
     */
    for(channel = 0; channel < deviceDesc->ADCChannels; channel++) {
        if((pGainCodes[ AD_CONFIG_GAIN_CODE + channel ] & ~AD_GAIN_CODE_MASK) != 0) {
            AIOUSB_UnLock();
            return AIOUSB_ERROR_INVALID_PARAMETER;
        }
    }

    retval = ReadConfigBlock( usb , AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

    if ( retval != AIOUSB_SUCCESS )
        goto out_ADC_RangeAll;

    if ( retval == AIOUSB_SUCCESS) {

          for(channel = 0; channel < deviceDesc->ADCChannels; channel++) {
              ADCConfigBlockSetGainCode( AIOUSBDeviceGetADCConfigBlock( deviceDesc) , channel, pGainCodes[ channel ] );
              ADCConfigBlockSetDifferentialMode( AIOUSBDeviceGetADCConfigBlock(deviceDesc), channel,
                                                 (bSingleEnded == AIOUSB_FALSE) ? AIOUSB_TRUE : AIOUSB_FALSE);
          }

          retval = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

      }
 out_ADC_RangeAll:
    if ( retval < AIOUSB_SUCCESS ) 
        result = -(retval);
    AIOUSB_UnLock();
    return result;
}

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param ADChannel
 * @param GainCode
 * @param bSingleEnded
 * @return
 */
AIORET_TYPE ADC_Range1(unsigned long DeviceIndex,
                       unsigned long ADChannel,
                       unsigned char RangeCode,
                       unsigned long bSingleEnded
                       )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT *)&retval );
    USBDevice *usb;

    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_valid_adc_stream( 
                                                     _check_adc_mux_channels( _check_range_and_ended_params(retval,deviceDesc,ADChannel,RangeCode,bSingleEnded ), deviceDesc ),
                                                     deviceDesc
                                                    ),
                             usb,  
                             out_ADC_Range1
                             );    

    retval = ReadConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

    if ( retval != AIOUSB_SUCCESS )
        goto out_ADC_Range1;
    
    ADCConfigBlockSetGainCode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), ADChannel, (ADGainCode)RangeCode);
    ADCConfigBlockSetDifferentialMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), ADChannel,
                                      (bSingleEnded == AIOUSB_FALSE) ? AIOUSB_TRUE : AIOUSB_FALSE);
    
    retval = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

out_ADC_Range1:
    AIOUSB_UnLock();
    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param TriggerMode
 * @param CalMode
 * @return
 */
AIORET_TYPE ADC_ADMode(
                         unsigned long DeviceIndex,
                         unsigned char TriggerMode,
                         unsigned char CalMode
                         )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT*)&retval );
    USBDevice *usb;

    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_valid_trigger( _check_cal_mode( retval, CalMode ), TriggerMode ),
                             usb,  
                             out_ADC_ADMode
                             );    
    
    retval = ReadConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
    
    if ( retval != AIOUSB_SUCCESS )
        goto out_ADC_ADMode;

#ifdef __cplusplus
    ADCConfigBlockSetCalMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), static_cast<ADCalMode>(CalMode) );
#else
    ADCConfigBlockSetCalMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), CalMode);
#endif
    ADCConfigBlockSetTriggerMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), TriggerMode);

    retval = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

 out_ADC_ADMode:

    AIOUSB_UnLock();
    return retval;
}

/**
 * @param DeviceIndex
 * @param Oversample
 * @return
 */
AIORET_TYPE ADC_SetOversample(
                                unsigned long DeviceIndex,
                                unsigned char Oversample
                                )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT*)&retval );
    USBDevice *usb;

    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_valid_adc_stream( retval, deviceDesc),
                             usb,  
                             out_ADC_SetOversample
                             );    

    retval = ReadConfigBlock( usb,  AIOUSBDeviceGetADCConfigBlock( deviceDesc ) );

    if( retval != AIOUSB_SUCCESS) 
        goto out_ADC_SetOversample;

    ADCConfigBlockSetOversample( AIOUSBDeviceGetADCConfigBlock( deviceDesc ) , Oversample);

    retval = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

out_ADC_SetOversample:
    AIOUSB_UnLock();
    return retval;
}

/**
 * @param DeviceIndex 
 * @return 
 */
AIORET_TYPE ADC_GetOversample( unsigned long DeviceIndex )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT *)&retval );
    USBDevice *usb;
    
    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             AIOUSB_SUCCESS,
                             usb,  
                             out_ADC_GetOversample
                             );    

    retval = ReadConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ) );

    if ( retval != AIOUSB_SUCCESS )
        goto out_ADC_GetOversample;

    retval = ADCConfigBlockGetOversample(AIOUSBDeviceGetADCConfigBlock( deviceDesc ) );
  
 out_ADC_GetOversample:
     AIOUSB_UnLock();
    return retval;

}

/*----------------------------------------------------------------------------*/
AIORET_TYPE ADC_SetAllGainCodeAndDiffMode( unsigned long DeviceIndex, unsigned gain, AIOUSB_BOOL differentialMode ) 
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT*)&retval );
    USBDevice *usb;

    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_valid_adc_stream( retval, deviceDesc ),
                             usb,  
                             out_ADC_SetAllGainCodeAndDiffMode
                             );
    
    retval = ADCConfigBlockSetAllGainCodeAndDiffMode( AIOUSBDeviceGetADCConfigBlock( deviceDesc ), gain, differentialMode );

 out_ADC_SetAllGainCodeAndDiffMode:
    AIOUSB_UnLock();
    return retval;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief
 * @param DeviceIndex
 * @param StartChannel
 * @param EndChannel
 * @return
 */
AIORET_TYPE ADC_SetScanLimits(
                              unsigned long DeviceIndex,
                              unsigned long StartChannel,
                              unsigned long EndChannel
                              )
{
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT*)&retval );
    USBDevice *usb;
    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_start_end_channels( retval,deviceDesc,StartChannel,EndChannel ),
                             usb,  
                             out_ADC_SetScanLimits
                             );        

    retval = ReadConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

    if ( retval !=  AIOUSB_SUCCESS )
        goto out_ADC_SetScanLimits;
    

    retval = ADCConfigBlockSetScanRange(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), StartChannel, EndChannel);
    if ( retval !=  AIOUSB_SUCCESS )
        goto out_ADC_SetScanLimits;
    
    retval = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

 out_ADC_SetScanLimits:
    AIOUSB_UnLock();
    return retval;
}

/**
 * @brief 
 * @param DeviceIndex
 * @param CalFileName
 * @return
 */
AIORET_TYPE ADC_SetCal(
    unsigned long DeviceIndex,
    const char *CalFileName
    )
{
    if(CalFileName == NULL)
        return AIOUSB_ERROR_INVALID_PARAMETER;

    assert(strlen(CalFileName) > 0);
    unsigned long result;
    if(strcmp(CalFileName, ":AUTO:") == 0)
        result = AIOUSB_ADC_InternalCal(DeviceIndex, AIOUSB_TRUE, 0, 0);
    else if(
        strcmp(CalFileName, ":NONE:") == 0 ||
        strcmp(CalFileName, ":1TO1:") == 0
        )
        result = AIOUSB_ADC_InternalCal(DeviceIndex, AIOUSB_FALSE, 0, 0);
    else
        result = AIOUSB_ADC_LoadCalTable(DeviceIndex, CalFileName);

    return result;
}



/**
 * @param DeviceIndex
 * @return
 */
AIORET_TYPE ADC_QueryCal(
                           unsigned long DeviceIndex
                           ) 
{

    unsigned char calSupported = 0xff;       // so we can detect if it changes
    int bytesTransferred;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    USBDevice *usb;
    if ( !AIOUSB_Lock() ) {
        result = AIOUSB_ERROR_INVALID_MUTEX;
        goto out_ADC_QueryCal;
    }
    if ( result != AIOUSB_SUCCESS )
        goto out_ADC_QueryCal;

    usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    if ( !usb ) {
        result = AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
        goto out_ADC_QueryCal;
    }


    AIOUSB_UnLock();                              // unlock while communicating with device
    bytesTransferred = usb->usb_control_transfer(usb, 
                                                 USB_READ_FROM_DEVICE, 
                                                 AUR_PROBE_CALFEATURE,
                                                 0, 
                                                 0, 
                                                 &calSupported, 
                                                 sizeof(calSupported), 
                                                 deviceDesc->commTimeout
                                                 );
    if (bytesTransferred == sizeof(calSupported)) {
        if(calSupported != 0xBB) /* 0xBB == AUR_LOAD_BULK_CALIBRATION_BLOCK */
            result = AIOUSB_ERROR_NOT_SUPPORTED;
    } else
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);

 out_ADC_QueryCal:
    AIOUSB_UnLock();
    return result;
}

/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param pConfigBuf
 * @param ConfigBufSize
 * @param CalFileName
 * @return
 */
AIORET_TYPE ADC_Initialize(
                             unsigned long DeviceIndex,
                             unsigned char *pConfigBuf,
                             unsigned long *ConfigBufSize,
                             const char *CalFileName
                             )
{
    unsigned long result = AIOUSB_SUCCESS;

    if(
        pConfigBuf != NULL &&
        ConfigBufSize != NULL
        )
        result = ADC_SetConfig(DeviceIndex, pConfigBuf, ConfigBufSize);

    if(
        result == AIOUSB_SUCCESS &&
        CalFileName != NULL
        )
        result = ADC_SetCal(DeviceIndex, CalFileName);

    return result;
}




static void *BulkAcquireWorker(void *params);

/**
 * @param DeviceIndex
 * @param BufSize
 * @param pBuf
 * @return
 */
AIORET_TYPE ADC_BulkAcquire(
                              unsigned long DeviceIndex,
                              unsigned long BufSize,
                              void *pBuf
                              )
{
    if(pBuf == NULL)
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

    if(deviceDesc->workerBusy) {
          AIOUSB_UnLock();
          return AIOUSB_ERROR_OPEN_FAILED;
      }

    AIOUSB_UnLock();
    struct BulkAcquireWorkerParams *const acquireParams
        = ( struct BulkAcquireWorkerParams* )malloc(sizeof(struct BulkAcquireWorkerParams));
    assert(acquireParams != 0);
    if(acquireParams != 0) {
      /**
       * we initialize the worker thread status here in case the thread doesn't start for some reason,
       * such as an improperly locked mutex; this pre-initialization is necessary so that the thread
       * status doesn't make it appear as though the worker thread has completed successfully
       */
          AIOUSB_Lock();
          deviceDesc->workerStatus = BufSize;       // deviceDesc->workerStatus == bytes remaining to receive
          deviceDesc->workerResult = AIOUSB_ERROR_INVALID_DATA;
          deviceDesc->workerBusy = AIOUSB_TRUE;
          AIOUSB_UnLock();
          acquireParams->DeviceIndex = DeviceIndex;
          acquireParams->BufSize = BufSize;
          acquireParams->pBuf = pBuf;
          const int maxPriority = sched_get_priority_max(SCHED_FIFO);
          struct sched_param schedParam = { maxPriority };
          pthread_attr_t workerThreadAttr;
          pthread_t workerThreadID;
          pthread_attr_init(&workerThreadAttr);
          pthread_attr_setschedpolicy(&workerThreadAttr, SCHED_FIFO);
          pthread_attr_setschedparam(&workerThreadAttr, &schedParam);
          const int threadResult = pthread_create(&workerThreadID, &workerThreadAttr, BulkAcquireWorker, acquireParams);
          if(threadResult == 0) {
                sched_yield();
            }else {
              /*
               * failed to create worker thread, clean up
               */
                AIOUSB_Lock();
                deviceDesc->workerStatus = 0;
                deviceDesc->workerResult = AIOUSB_SUCCESS;
                deviceDesc->workerBusy = AIOUSB_FALSE;
                AIOUSB_UnLock();
                free(acquireParams);
                result = AIOUSB_ERROR_INVALID_THREAD;
            }
          pthread_attr_destroy(&workerThreadAttr);
          pthread_detach(workerThreadID);
      }else
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    return result;
}
/*----------------------------------------------------------------------------*/
/**
 * @brief we assume the parameters passed to BulkAcquireWorker() have
 * been validated by ADC_BulkAcquire()
 * @param params
 * @return
 */
static void *BulkAcquireWorker(void *params)
{
    assert(params != 0);
    unsigned long result = AIOUSB_SUCCESS;

    struct BulkAcquireWorkerParams *const acquireParams = ( struct BulkAcquireWorkerParams* )params;
    double clockHz;
    unsigned long bytesRemaining, streamingBlockSize ,bytesToTransfer;
    unsigned short numSamplesHigh, numSamplesLow;
    unsigned char *data;
    int bytesTransferred,libusbResult;
    USBDevice *usb;
    AIOUSB_Lock();
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( acquireParams->DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ) {
        result = AIOUSB_ERROR_DEVICE_NOT_CONNECTED;
        goto out_BulkAcquireWorker;
    }
    usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    if (!usb ) {
        result = AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
        goto out_BulkAcquireWorker;
    }

    bytesRemaining = acquireParams->BufSize;
    deviceDesc->workerStatus = bytesRemaining;       // deviceDesc->workerStatus == bytes remaining to receive
    deviceDesc->workerResult = AIOUSB_SUCCESS;
    deviceDesc->workerBusy = AIOUSB_TRUE;
    clockHz = deviceDesc->miscClockHz;
    streamingBlockSize = deviceDesc->StreamingBlockSize * sizeof(unsigned short);       // bytes

    AIOUSB_UnLock();                              // unlock while communicating with device
    numSamplesHigh = ( unsigned short )(acquireParams->BufSize >> 17);       // acquireParams->BufSize is bytes
    numSamplesLow = ( unsigned short )(acquireParams->BufSize >> 1);
    
    data = ( unsigned char* )acquireParams->pBuf;
    
    bytesTransferred = usb->usb_control_transfer(usb,
                                                 USB_WRITE_TO_DEVICE, 
                                                 AUR_START_ACQUIRING_BLOCK,
                                                 numSamplesHigh, 
                                                 numSamplesLow, 
                                                 0, 
                                                 0, 
                                                 deviceDesc->commTimeout
                                                 );
    if (bytesTransferred != 0) {
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
        goto out_BulkAcquireWorker;
    }

    CTR_StartOutputFreq(acquireParams->DeviceIndex, 0, &clockHz);

    while(bytesRemaining > 0) {
        bytesToTransfer = (bytesRemaining < streamingBlockSize)? bytesRemaining: streamingBlockSize;
        /* libusbResult = AIOUSB_BulkTransfer(deviceHandle, */
        /*                                    LIBUSB_ENDPOINT_IN | USB_BULK_READ_ENDPOINT, */
        /*                                    data,  */
        /*                                    ( int )bytesToTransfer,  */
        /*                                    &bytesTransferred, */
        /*                                    deviceDesc->commTimeout */
        /*                                    ); */
        libusbResult = usb->usb_bulk_transfer( usb,
                                               LIBUSB_ENDPOINT_IN | USB_BULK_READ_ENDPOINT,
                                               data,
                                               ( int )bytesToTransfer,
                                               &bytesTransferred,
                                               deviceDesc->commTimeout
                                               );

        if (libusbResult != LIBUSB_SUCCESS) {
            result = LIBUSB_RESULT_TO_AIOUSB_RESULT(libusbResult);
            break;
        } else if(bytesTransferred != ( int )bytesToTransfer) {
            result = AIOUSB_ERROR_INVALID_DATA;
            break;
        } else {
            data += bytesTransferred;
            bytesRemaining -= bytesTransferred;
            AIOUSB_Lock();
            deviceDesc->workerStatus = bytesRemaining;
            AIOUSB_UnLock();
        }
    }
    clockHz = 0;
    CTR_StartOutputFreq(acquireParams->DeviceIndex, 0, &clockHz);
    
 out_BulkAcquireWorker:
    AIOUSB_Lock();
    deviceDesc->workerStatus = 0;
    deviceDesc->workerResult = result;
    deviceDesc->workerBusy = AIOUSB_FALSE;
    AIOUSB_UnLock();
    free(params);
    return 0;
}
/*----------------------------------------------------------------------------*/
AIOBuf *
NewBuffer( unsigned int bufsize )
{
     AIOBuf *tmp = (AIOBuf*)malloc( sizeof( AIOBuf) );
     if( !tmp ) {
          aio_errno = -AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
          goto out_ret;
     }

     tmp->bytes_remaining = 0;
     tmp->bufsize = bufsize;
  
     /* printf("allocating space %d bytes\n",(int)tmp->bufsize ); */
     tmp->buffer = (unsigned short *)malloc( tmp->bufsize );
     if( !tmp->buffer ) {
          aio_errno = -AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
          free(tmp);
          tmp = NULL;
     }
out_ret:
     return tmp;
}

void
DeleteBuffer( AIOBuf *buf )
{
  if( !buf ) 
    return;

  if( buf->buffer ) 
    free(buf->buffer );
  free(buf);
}

/** 
 * @brief After setting up your oversamples and such, creates a new
 * AIOBuf object that can be used for BulkAcquiring.
 * @param DeviceIndex 
 * @return AIOBuf * new Buffer object for BulkAcquire methods
 * @todo Replace 16 with correct channels returned by probing the device
 */
AIOBuf *CreateSmartBuffer( unsigned long DeviceIndex )
{
  AIORESULT result;
  AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex(DeviceIndex, &result);
  if(!deviceDesc || result != AIOUSB_SUCCESS) {
       aio_errno = -result;
       return (AIOBuf*)NULL;
  }
  ADCConfigBlock *config = AIOUSBDeviceGetADCConfigBlock( deviceDesc );

  long size = ((1 + ADCConfigBlockGetOversample( config )) * 16 * sizeof( unsigned short ) * AIOUSB_GetStreamingBlockSize(DeviceIndex)) ;
  AIOBuf *tmp = NewBuffer( size );
  
  return tmp;
}

AIORET_TYPE  
BulkAcquire(
            unsigned long DeviceIndex,
            AIOBuf *aiobuf,
            int size
            )
{
  AIORET_TYPE result = 0;
  result = ADC_BulkAcquire( DeviceIndex, aiobuf->bufsize , aiobuf->buffer );
  aiobuf->bytes_remaining = aiobuf->bufsize;
  return result;

}


AIORET_TYPE  
BulkPoll(
         unsigned long DeviceIndex,
         AIOBuf *aiobuf
         )
{
  AIORET_TYPE result= AIOUSB_SUCCESS;
  unsigned long retval = ADC_BulkPoll( DeviceIndex, &(aiobuf->bytes_remaining) );
  if( retval != AIOUSB_SUCCESS ) 
    result = - retval;

  return result;
}





/**
 * @param DeviceIndex
 * @param BytesLeft
 * @return
 */
AIORET_TYPE ADC_BulkPoll(
    unsigned long DeviceIndex,
    unsigned long *BytesLeft
    )
{
    if(BytesLeft == NULL)
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

    *BytesLeft = deviceDesc->workerStatus;
    result = deviceDesc->workerResult;
    AIOUSB_UnLock();

    return result;
}



/**
 * @brief
 * o this function is erroneously documented in the API specification, but it should
 *   not be made public; it is useful internally, however, for such things as
 *   calibrating the A/D converter
 *
 * o the specification does not include a Channel parameter, but this implementation
 *   does because the Pascal code does and because it's used by ADC_SetCal()
 *
 * o in a departure from the Pascal code, this function supports two categories of
 *   "immediate" A/Ds: the older products which have a single immediate A/D channel,
 *   and the newer products which have multiple immediate A/D channels; fortunately,
 *   this function accepts a pData parameter, which permits us to return any amount of
 *   data; the caller simply has to make sure that his pData buffer is large enough for
 *   the particular device; that's not an unreasonable demand since this function is
 *   used internally and not intended to be public
 */
static AIORESULT ADC_GetImmediate(
                                  unsigned long DeviceIndex,
                                  unsigned long Channel,
                                  unsigned short *pData
                                  )
{

    
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    int numBytes;
    int bytesTransferred;
    USBDevice *usb;
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }
    if (pData == NULL) {
        result =  AIOUSB_ERROR_INVALID_PARAMETER;
        goto out_ADC_GetImmediate;
    }

    if(!AIOUSB_Lock()) {
        result =  AIOUSB_ERROR_INVALID_MUTEX;
        goto out_ADC_GetImmediate;
    }

    if(deviceDesc->ImmADCs == 0) {
        result = AIOUSB_ERROR_NOT_SUPPORTED;
        goto out_ADC_GetImmediate;
    }

    usb = AIOUSBDeviceGetUSBHandle( deviceDesc );
    if (!usb ) {
        result = AIOUSB_ERROR_USBDEVICE_NOT_FOUND;
        goto out_ADC_GetImmediate;
    }

    numBytes = sizeof(unsigned short) * deviceDesc->ImmADCs;

    AIOUSB_UnLock();

    bytesTransferred = usb->usb_control_transfer(usb, 
                                                 USB_READ_FROM_DEVICE, 
                                                 AUR_ADC_IMMEDIATE,
                                                 0, 
                                                 Channel, 
                                                 ( unsigned char* )pData, 
                                                 numBytes, 
                                                 deviceDesc->commTimeout
                                                 );
    if(bytesTransferred != numBytes)
        result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);

 out_ADC_GetImmediate:
    AIOUSB_UnLock();
    return result;
}


/*----------------------------------------------------------------------------*/
/**
 * @brief Creates FastIT Config Blocks
 * @param DeviceIndex
 * @param size
 * @return
 */
AIORET_TYPE ADC_CreateFastITConfig(unsigned long DeviceIndex,
                                     int size
                                     )
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    if(!deviceDesc->FastITConfig || deviceDesc->FastITConfig->size <= 0) {
          deviceDesc->FastITConfig = (ADCConfigBlock*)malloc(sizeof(ADCConfigBlock));
          deviceDesc->FastITBakConfig = (ADCConfigBlock*)malloc(sizeof(ADCConfigBlock));
          deviceDesc->FastITConfig->size = size;
          deviceDesc->FastITBakConfig->size = size;
      }
    return AIOUSB_SUCCESS;
}


/*----------------------------------------------------------------------------*/
unsigned char *ADC_GetADCConfigBlock_Registers(ADCConfigBlock *config)
{
    return &(config->registers[0]);
}



/**
 * @brief Frees memory associated with the FastConfig Config blocks. Use
 *       this call after you are done using the ADC_FastIT* Functions
 * @param DeviceIndex
 */
/*----------------------------------------------------------------------------*/
void ADC_ClearFastITConfig(unsigned long DeviceIndex)
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS )
        return ;

    if(deviceDesc->FastITConfig->size) {
          free(deviceDesc->FastITConfig);
          free(deviceDesc->FastITBakConfig);
          deviceDesc->FastITConfig = NULL;
          deviceDesc->FastITBakConfig = NULL;
      }
}



/*----------------------------------------------------------------------------*/
AIORET_TYPE ADC_CreateADBuf(AIOUSBDevice *const deviceDesc,
                            int size
                            )
{
    deviceDesc->ADBuf = (unsigned char*)malloc(sizeof(unsigned char*) * size);
    if(!deviceDesc->ADBuf) {
          return 0;
      }
    deviceDesc->ADBuf_size = size;
    return AIOUSB_SUCCESS;
}
/*----------------------------------------------------------------------------*/
void ADC_ClearADBuf(AIOUSBDevice *deviceDesc)
{
    if(deviceDesc->ADBuf_size) {
          free(deviceDesc->ADBuf);
          deviceDesc->ADBuf_size = 0;
      }
}
/*----------------------------------------------------------------------------*/
AIORET_TYPE ADC_InitFastITScanV(
    unsigned long DeviceIndex
    )
{
    int Dat;

    if(!AIOUSB_Lock())
        return AIOUSB_ERROR_INVALID_MUTEX;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    result = AIOUSB_EnsureOpen(DeviceIndex);
    if(result != AIOUSB_SUCCESS) {
          AIOUSB_UnLock();
          goto RETURN_ADC_InitFastITScanV;
      }

    if((result = ADC_CreateFastITConfig(DeviceIndex, 21)) != AIOUSB_SUCCESS)
        goto CLEAR_CONFIG_ADC_InitFastITScanV;


    if(deviceDesc->FastITConfig->size < 20) {
          result = AIOUSB_ERROR_BAD_TOKEN_TYPE;
          goto RETURN_ADC_InitFastITScanV;
      }

    result = ADC_GetConfig(DeviceIndex, &deviceDesc->FastITBakConfig->registers[0], &deviceDesc->FastITBakConfig->size);

    if(result != AIOUSB_SUCCESS) {
          AIOUSB_UnLock();
          goto RETURN_ADC_InitFastITScanV;
      }

    /* Copy old config */
    memcpy(&deviceDesc->FastITConfig->registers[0], &deviceDesc->FastITBakConfig->registers[0], deviceDesc->FastITBakConfig->size);

    /* Make changes to the config block */
    Dat = AIOUSB_GetRegister(deviceDesc->FastITBakConfig, 0x10);
    AIOUSB_SetRegister(deviceDesc->FastITConfig, 0x11, (0x05 | (Dat & 0x10)));
    Dat = (3 > AIOUSB_GetRegister(deviceDesc->FastITBakConfig, 0x13) ? 3 : AIOUSB_GetRegister(deviceDesc->FastITBakConfig, 0x13));

    AIOUSB_SetRegister(deviceDesc->FastITConfig, 0x13, Dat);
    Dat = 64 > deviceDesc->ADCMUXChannels ?  deviceDesc->ADCMUXChannels - 1  : 63;

    AIOUSB_SetRegister(deviceDesc->FastITConfig, 0x12, Dat << 4);
    AIOUSB_SetRegister(deviceDesc->FastITConfig, 0x14, Dat & 0xF0);


    result = ADC_SetConfig(DeviceIndex, &deviceDesc->FastITConfig->registers[0], &deviceDesc->FastITConfig->size);

    if(result != AIOUSB_SUCCESS) {
          ADC_SetConfig(DeviceIndex, ADC_GetADCConfigBlock_Registers(deviceDesc->FastITBakConfig), &deviceDesc->FastITConfig_size);
          return result;
      }

    result = 0;

CLEAR_CONFIG_ADC_InitFastITScanV:
    if(result != AIOUSB_SUCCESS) {
        if(deviceDesc->FastITConfig_size)
            ADC_ClearFastITConfig(DeviceIndex);
    }

RETURN_ADC_InitFastITScanV:
    AIOUSB_UnLock();
    return result;
}
/*----------------------------------------------------------------------------*/
AIORET_TYPE ADC_ResetFastITScanV(
    unsigned long DeviceIndex
    )
{

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    if(!deviceDesc->bADCStream || deviceDesc->ConfigBytes < 20) {
        result = AIOUSB_ERROR_BAD_TOKEN_TYPE;
        goto RETURN_ADC_ResetFastITScanV;
    }
    result = ADC_SetConfig(DeviceIndex, ADC_GetADCConfigBlock_Registers(deviceDesc->FastITBakConfig), &deviceDesc->ConfigBytes);
    if(result != AIOUSB_SUCCESS)
        goto RETURN_ADC_ResetFastITScanV;

    /* Dat = 0x0; */
    /* result = GenericVendorWrite( DeviceIndex, 0xD4, 0x1E, 0, sizeof(Dat), &Dat ); */
    ADC_ClearFastITConfig(DeviceIndex);

RETURN_ADC_ResetFastITScanV:
    return result;
}
/*----------------------------------------------------------------------------*/
AIORET_TYPE ADC_SetFastITScanVChannels(
                                         unsigned long DeviceIndex,
                                         unsigned long NewChannels
                                         )
{
    ADCConfigBlock configBlock;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    configBlock.device = deviceDesc;
    configBlock.size = deviceDesc->ConfigBytes;

    result = AIOUSB_EnsureOpen(DeviceIndex);
    if(result != AIOUSB_SUCCESS)
        goto RETURN_ADC_SetFastITScanVChannels;

    if(!deviceDesc->bADCStream) {
          result = AIOUSB_ERROR_BAD_TOKEN_TYPE;
          goto RETURN_ADC_SetFastITScanVChannels;
      }

    if(configBlock.size < 20) {
          result = AIOUSB_ERROR_BAD_TOKEN_TYPE;
          goto RETURN_ADC_SetFastITScanVChannels;
      }

    result = ADC_SetConfig(DeviceIndex, ADC_GetADCConfigBlock_Registers(deviceDesc->FastITConfig), &deviceDesc->ConfigBytes);

RETURN_ADC_SetFastITScanVChannels:
    return result;
}
/*----------------------------------------------------------------------------*/
/**
 * @brief Just a debugging function for listing all attributes of a 
 *       config object
 **/
void ADC_Debug_Register_Settings(ADCConfigBlock *config)
{
    int i;

    for(i = 0; i <= 15; i++) {
          printf("Channel %d:\t", i);
          switch(config->registers[i]) {
            case FIRST_ENUM(ADGainCode):
                printf("0-10V\n");
                break;

            case AD_GAIN_CODE_10V:
                printf("+/-10V\n");
                break;

            case AD_GAIN_CODE_0_5V:
                printf("0-5V\n");
                break;

            case AD_GAIN_CODE_5V:
                printf("+/-5V\n");
                break;

            case AD_GAIN_CODE_0_2V:
                printf("0-2V\n");
                break;

            case AD_GAIN_CODE_2V:
                printf("+/-2V\n");
                break;

            case AD_GAIN_CODE_0_1V:
                printf("0-1V\n");
                break;

            case AD_GAIN_CODE_1V:
                printf("+/-1V\n");
                break;

            default:
                printf("Unknown\n");
            }
      }

    printf("Calibration Mode:\t");

    switch(config->registers[AD_REGISTER_CAL_MODE]) {
      case AD_CAL_MODE_NORMAL:
          printf("Normal\n");
          break;

      case AD_CAL_MODE_GROUND:
          printf("Ground\n");
          break;

      case AD_CAL_MODE_REFERENCE:
          printf("Reference\n");
          break;

      case AD_CAL_MODE_BIP_GROUND:
          printf("BIP Reference\n");
          break;

      default:
          printf("Unknown\n");
      }

    printf("Trig/Counter clk\t");
    if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_CTR0_EXT) {
          printf("(counter)              ");
      }else {
          printf("(externally triggered) ");
      }

    if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_FALLING_EDGE) {
          printf("(triggered by falling edge) ");
      }

    if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_SCAN) {
          printf("(scan all channels) ");
      }else {
          printf("(single channel) ");
      }

    if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_EXTERNAL) {
          printf("(triggered by external ) ");
      }else if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_TIMER) {
          printf("(triggered by counter 2) ");
      }else
        printf("(triggered by sw       ) ");

    printf("\n");

    /* Display the Start end channels */

    printf("Channels:\tstart=%d, end=%d\n", config->registers[AD_CONFIG_START_END] & 0xF, config->registers[AD_CONFIG_START_END] >> 4);
}
/*----------------------------------------------------------------------------*/
/**
 * @verbatim
 * ---
 * config:
 *   channels:
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   - gain: 0-10V
 *   calibration: Normal
 *   trigger:
 *     edge: falling edge
 *     scan: all channels
 *     type: external
 * @endverbatim
 */
char * ADCConfigBlockToYAML(ADCConfigBlock *config)
{
    int i;
    char tmpbuf[2048] = {0};
    char tbuf[5];
    strcat(tmpbuf,"---\nconfig:\n");
    strcat(tmpbuf,"  channels:\n");
    for(i = 0; i <= 15; i++) {
        strcat(tmpbuf,"  - gain: ");
        switch(config->registers[i]) {
            case FIRST_ENUM(ADGainCode):
                strcat(tmpbuf, "0-10V\n");
                break;
            case AD_GAIN_CODE_10V:
                strcat(tmpbuf, "+/-10V\n");
                break;
            case AD_GAIN_CODE_0_5V:
                strcat(tmpbuf, "0-5V\n");
                break;
            case AD_GAIN_CODE_5V:
                strcat(tmpbuf, "+/-5V\n");
                break;
            case AD_GAIN_CODE_0_2V:
                strcat(tmpbuf, "0-2V\n");
                break;
            case AD_GAIN_CODE_2V:
                strcat(tmpbuf, "+/-2V\n");
                break;
            case AD_GAIN_CODE_0_1V:
                strcat(tmpbuf, "0-1V\n");
                break;
            case AD_GAIN_CODE_1V:
                strcat(tmpbuf, "+/-1V\n");
                break;
            default:
                strcat(tmpbuf, "Unknown\n");
        }
    }

    strcat(tmpbuf,"  calibration: ");
    switch (config->registers[AD_REGISTER_CAL_MODE] ) {
      case AD_CAL_MODE_NORMAL:
          strcat(tmpbuf,"Normal\n");
          break;

      case AD_CAL_MODE_GROUND:
          strcat(tmpbuf,"Ground\n");
          break;

      case AD_CAL_MODE_REFERENCE:
          strcat(tmpbuf, "Reference\n");
          break;

      case AD_CAL_MODE_BIP_GROUND:
          strcat(tmpbuf,"BIP Reference\n");
          break;

      default:
          strcat(tmpbuf, "Unknown\n");
      }

    strcat(tmpbuf, "  trigger: ");
    if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_CTR0_EXT) {
        strcat(tmpbuf, "counter ");
    } else {
        strcat(tmpbuf, "external ");
    }
    if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_FALLING_EDGE) {
        strcat(tmpbuf, "falling edge");
    } else {
        strcat( tmpbuf, "rising edge" );
    }

    if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_SCAN) {
        strcat(tmpbuf, "all channels");
    } else {
        strcat(tmpbuf, "single channel");
    }
    strcat(tmpbuf,"    type: ");
    if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_EXTERNAL) {
        strcat(tmpbuf,"external\n");
    } else if (config->registers[AD_REGISTER_TRIG_COUNT] & AD_TRIGGER_TIMER) {
        strcat(tmpbuf,"counter\n");
    } else {
        strcat(tmpbuf,"sw\n");
    }
    strcat(tmpbuf,  "start_channel: ");
    sprintf(tbuf,"%d\n", config->registers[AD_CONFIG_START_END] & 0xF );
    strcat(tmpbuf, tbuf );
    strcat(tmpbuf,  "end_channel: ");
    sprintf(tbuf,"%d\n", config->registers[AD_CONFIG_START_END] >> 4 );
    strcat(tmpbuf, tbuf );

    return strdup(tbuf);
}
/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param pBuf
 * @return
 */
AIORET_TYPE ADC_GetFastITScanV(unsigned long DeviceIndex, double *pData)
{
    int StartChannel;
    int EndChannel;
    int Channels;
    unsigned long BytesLeft;
    unsigned short *thisDataBuf;

    int bufsize;
    double clockHz = 0;
    double *pBuf;
    int numsleep = 100;
    double CLOCK_SPEED = 100000;
    int i, ch;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    if(!deviceDesc->bADCStream) {
          result = AIOUSB_ERROR_BAD_TOKEN_TYPE;
          goto out_ADC_GetFastITScanV;
      }

    if(deviceDesc->ConfigBytes < 20) {
          result = AIOUSB_ERROR_BAD_TOKEN_TYPE;
          goto out_ADC_GetFastITScanV;
      }

    StartChannel  = AIOUSB_GetRegister(deviceDesc->FastITConfig, 0x12) & 0x0F;
    EndChannel    = AIOUSB_GetRegister(deviceDesc->FastITConfig, 0x12) >> 4;

    if( deviceDesc->ConfigBytes >= 21 ) {
        StartChannel = StartChannel  | ((AIOUSB_GetRegister(deviceDesc->FastITConfig, 20 ) & 0xF ) << 4);
        EndChannel   = EndChannel    | ( AIOUSB_GetRegister( deviceDesc->FastITConfig, 20 ) & 0xF0 );
    }


    Channels      = EndChannel - StartChannel + 1;

    /*  Result := ADC_BulkAcquire(DeviceIndex, Length(ADBuf) * SizeOf(ADBuf[0]), @ADBuf[0]); */

    CTR_8254Mode(DeviceIndex, 0, 2, 0);
    CTR_8254Mode(DeviceIndex, 0, 2, 1);
    bufsize = Channels * sizeof(unsigned short) * (AIOUSB_GetRegister(deviceDesc->FastITConfig, 0x13) + 1); /* 1 sample + 3 oversamples */;

    clockHz = 0;

    ADC_SetScanLimits(DeviceIndex, 0, Channels - 1);

    CLOCK_SPEED = 100000;       // Hz
    AIOUSB_SetStreamingBlockSize(DeviceIndex, 100000);
    thisDataBuf = ( unsigned short* )malloc(bufsize + 100);
    memset(thisDataBuf, 0, bufsize + 100);

    clockHz = 0;
    CTR_StartOutputFreq(DeviceIndex, 0, &clockHz);
    ADC_ADMode(DeviceIndex, AD_TRIGGER_SCAN | AD_TRIGGER_TIMER, AD_CAL_MODE_NORMAL);
    AIOUSB_SetMiscClock(DeviceIndex, CLOCK_SPEED);


    result = ADC_BulkAcquire(DeviceIndex, bufsize, thisDataBuf);

    if(result != AIOUSB_SUCCESS)
        goto CLEANUP_ADC_GetFastITScanV;

    BytesLeft = bufsize;
    numsleep = 0;
    usleep(0);
    while(BytesLeft) {
          result = ADC_BulkPoll(DeviceIndex, &BytesLeft);
          if(result != AIOUSB_SUCCESS) {
                break;
            }else {
                numsleep++;
                usleep(10);
                if(numsleep > 100) {
                      result = AIOUSB_ERROR_TIMEOUT;
                      break;
                  }
            }
      }

    if(result != AIOUSB_SUCCESS)
        goto CLEANUP_ADC_GetFastITScanV;

    pBuf = pData;

    for(i = 0, ch = StartChannel; ch <= EndChannel; i++, ch++) {
          int RangeCode = AIOUSB_GetRegister(deviceDesc->FastITConfig, ch >> deviceDesc->RangeShift);
          int Tot = 0, Wt = 0;
          float V;
          int j;
          int numsamples = AIOUSB_GetRegister(deviceDesc->FastITConfig, 0x13) + 1;

          for(j = 1; j < numsamples; j++) {
                Tot += thisDataBuf[i * (numsamples) + j];
                Wt++;
          }
          V = Tot / Wt / (float)65536;
          if((RangeCode & 1) != 0)
              V = V * 2 - 1;
          if((RangeCode & 2) == 0)
              V = V * 2;
          if((RangeCode & 4) == 0)
              V = V * 5;

          *pBuf = (double)V;
          pBuf++;

          fflush(stdout);
      }

CLEANUP_ADC_GetFastITScanV:
    free(thisDataBuf);

out_ADC_GetFastITScanV:
    return result;
}
/*----------------------------------------------------------------------------*/
/**
 * @brief
 * @param DeviceIndex
 * @param pBuf
 * @return
 */
AIORET_TYPE ADC_GetITScanV(unsigned long DeviceIndex,
                             double *pBuf
                             )
{
    unsigned result = ADC_InitFastITScanV(DeviceIndex);

    result = ADC_GetFastITScanV(DeviceIndex, pBuf);
    if(result != AIOUSB_SUCCESS)
        result = ADC_ResetFastITScanV(DeviceIndex);

    return result;
}
/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @return
 */
AIORET_TYPE AIOUSB_IsDiscardFirstSample(
                                        unsigned long DeviceIndex
                                        )
{
    AIORET_TYPE retval = (AIORET_TYPE)AIOUSB_FALSE;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT *)&retval );

    if ( retval != AIOUSB_SUCCESS )
        return -AIOUSB_ERROR_DEVICE_NOT_FOUND;

    retval = (AIORET_TYPE)deviceDesc->discardFirstSample;

    return retval;
}
/*----------------------------------------------------------------------------*/
/**
 * @param DeviceIndex
 * @param discard
 * @return
 */
AIORET_TYPE AIOUSB_SetDiscardFirstSample(
                                           unsigned long DeviceIndex,
                                           AIOUSB_BOOL discard
                                           )
{
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ) 
        goto out_AIOUSB_SetDiscardFirstSample;

    deviceDesc->discardFirstSample = discard;

out_AIOUSB_SetDiscardFirstSample:
    AIOUSB_UnLock();
    return result;
}
/*----------------------------------------------------------------------------*/
void AIOUSB_Copy_Config_Block(ADCConfigBlock *to, ADCConfigBlock *from)
{
    to->device = from->device;
    to->size = from->size;
    memcpy(&to->registers[0], &from->registers[0], to->size);
}
/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSB_Validate_ADC_Device(unsigned long DeviceIndex)
{
    unsigned long result;

    if( (result = AIOUSB_Validate_Device( DeviceIndex ) ) != AIOUSB_SUCCESS )
        goto RETURN_AIOUSB_Validate_ADC_Device;
    
    result = ADC_QueryCal(DeviceIndex);

RETURN_AIOUSB_Validate_ADC_Device:
    AIOUSB_UnLock();
    return result;
}
/**
 * @param deviceIndex
 * @return
 */
/*----------------------------------------------------------------------------*/
double GetHiRef(unsigned long deviceIndex)
{
    const double HiRefRef = 65130.249;          // == 9.938239V on 0-10V range (9.938239V / 10.0V * 65535 = 65130.249)
    unsigned short RefData = 0xFFFF;
    unsigned long DataSize = sizeof(RefData);
    unsigned long Status = GenericVendorRead(deviceIndex, 0xA2, 0x1DF2, 0, &RefData, &DataSize );

    if(Status != AIOUSB_SUCCESS)
        return HiRefRef;
    if(DataSize != sizeof(RefData))
        return HiRefRef;
    if((RefData == 0xFFFF) || (RefData == 0x0000))
        return HiRefRef;
    return RefData;
}
/*----------------------------------------------------------------------------*/
/**
 * @brief Loads the Cal table for Automatic internal calibration
 * @param calTable
 * @param DeviceIndex
 * @param groundCounts
 * @param referenceCounts
 */
void DoLoadCalTable(
                    unsigned short *const calTable,
                    unsigned long DeviceIndex,
                    double groundCounts,
                    double referenceCounts
                    )
{
    const double TARGET_GROUND_COUNTS = 0;      // == 0.0V on 0-10V range (0.0V / 10.0V * 65535 = 0.0)
    const double TARGET_REFERENCE_COUNTS = GetHiRef(DeviceIndex);
    const double slope
        = (TARGET_REFERENCE_COUNTS - TARGET_GROUND_COUNTS)
          / (referenceCounts - groundCounts);
    const double offset = TARGET_GROUND_COUNTS - slope * groundCounts;
    int index;

    for(index = 0; index < CAL_TABLE_WORDS; index++) {
          long value = ( long )round(slope * index + offset);
          if(value < 0)
              value = 0;
          else if(value > AI_16_MAX_COUNTS)
              value = AI_16_MAX_COUNTS;
          calTable[ index ] = ( unsigned short )value;
      }
}
/*----------------------------------------------------------------------------*/
/**
 * @brief 
 * @param config
 * @param channel
 * @param gainCode
 */
AIORET_TYPE AIOUSB_SetRangeSingle(ADCConfigBlock *config, unsigned long channel, unsigned char gainCode)
{
    return ADCConfigBlockSetRangeSingle( config, channel, gainCode );
}
/*----------------------------------------------------------------------------*/
/**
 * @brief Performs automatic calibration of the ADC
 * @param DeviceIndex
 * @param autoCal
 * @param returnCalTable
 * @param saveFileName
 * @return
 */
AIORET_TYPE AIOUSB_ADC_InternalCal(
                                     unsigned long DeviceIndex,
                                     AIOUSB_BOOL autoCal,
                                     unsigned short returnCalTable[],
                                     const char *saveFileName
                                     )
{
    if(!AIOUSB_Lock())
        return AIOUSB_ERROR_INVALID_MUTEX;
    AIORET_TYPE retval = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, (AIORESULT*)&retval );
    USBDevice *usb;
    double groundCounts = 0, referenceCounts = 0, averageCounts;
    int reading, k ;
    unsigned short *calTable;

    EXIT_FN_IF_NO_VALID_USB( deviceDesc, 
                             retval, 
                             _check_valid_adc_stream(  _check_query_cal( retval, DeviceIndex ), deviceDesc ),
                             usb,  
                             out_AIOUSB_ADC_InternalCal
                             );            

    calTable = ( unsigned short* )malloc(CAL_TABLE_WORDS * sizeof(unsigned short));

    if ( !calTable ) {
        retval = -AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
        goto out_AIOUSB_ADC_InternalCal;
    }

    if ( 1 ) {
      /*
       * create calibrated calibration table
       */
          retval = ReadConfigBlock( usb,  AIOUSBDeviceGetADCConfigBlock( deviceDesc ) );

          if(retval == AIOUSB_SUCCESS) {

                /* const ADCConfigBlock origConfigBlock = deviceDesc->cachedConfigBlock;         // restore when done */
              ADCConfigBlock origConfigBlock;
              ADCConfigBlockCopy( &origConfigBlock, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

              ADCConfigBlockSetAllGainCodeAndDiffMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), AD_GAIN_CODE_0_10V, AIOUSB_FALSE);
              ADCConfigBlockSetTriggerMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), 0 );
              ADCConfigBlockSetScanRange(AIOUSBDeviceGetADCConfigBlock( deviceDesc )  , 0, 0 );
              ADCConfigBlockSetOversample(AIOUSBDeviceGetADCConfigBlock( deviceDesc  ), 0 );
              
              /* ADC_Range1( DeviceIndex , 0x00 , 0x01, AIOUSB_FALSE ); */
              int rangeChannel = 0x00;
              int rangeValue = DAC_RANGE_10V;
    
              /* See page 21 of the USB manual */
              ADCConfigBlockSetCalMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), AD_CAL_MODE_BIP_GROUND);         // select bip low, to select
              ADCConfigBlockSetRangeSingle(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), rangeChannel, rangeValue);   // Select +-10 range for channel 0
              
              /* WriteConfigBlock( DeviceIndex ); */

              for(reading = 0; reading <= 1; reading++) {
                  AIOUSB_Lock();
                  ADCConfigBlockSetCalMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), (reading == 0) ? AD_CAL_MODE_GROUND : AD_CAL_MODE_REFERENCE);
                  
                  retval = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
                  
                  if(retval == AIOUSB_SUCCESS) {
                      /*
                       * average a bunch of readings to get a nice, stable reading
                       */
                      const int AVERAGE_SAMPLES = 256;
                      const unsigned MAX_GROUND = 0x00ff,
                          MIN_REFERENCE = 0xf000;
                      long countsSum = 0;
                      int sample;
                      unsigned short counts[ MAX_IMM_ADCS ];
                      for(sample = 0; sample < AVERAGE_SAMPLES; sample++) {
                          retval = ADC_GetImmediate(DeviceIndex, 0, counts);
                          if(retval == AIOUSB_SUCCESS)
                              countsSum += counts[ 0 ];
                          else
                              goto abort;
                      }
                      averageCounts = countsSum / ( double )AVERAGE_SAMPLES;
                      if(reading == 0) {
                          if(averageCounts <= MAX_GROUND)
                              groundCounts = averageCounts;
                          else{
                              retval = -AIOUSB_ERROR_INVALID_DATA;
                              goto abort;
                          }             /* if( averageCounts ...*/
                      }else {
                          if(
                             averageCounts >= MIN_REFERENCE &&
                             averageCounts <= AI_16_MAX_COUNTS
                             )
                              referenceCounts = averageCounts;
                          else{
                              retval = -AIOUSB_ERROR_INVALID_DATA;
                              goto abort;
                          }
                      }
                  } else
                      goto abort;
              }
abort:
              ADCConfigBlockCopy( AIOUSBDeviceGetADCConfigBlock( deviceDesc ), &origConfigBlock );
                /* AIOUSB_Lock(); */
                /* deviceDesc->cachedConfigBlock = origConfigBlock; */
                /* AIOUSB_UnLock();                    // unlock while communicating with device */

              for (k = 0; k <= 1; k++) {

                  retval = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));

                  ADCConfigBlockSetTriggerMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), AD_TRIGGER_SCAN); /* scan software start */
                  ADCConfigBlockSetScanRange(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), 0, 0);              /* Select one channel */
                  ADCConfigBlockSetOversample(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), 0xff);             /* +255 oversample channel */
                  
                  if (k == 0)
                      ADCConfigBlockSetCalMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), AD_CAL_MODE_BIP_GROUND);  /* select bip low, to select*/
                  
                  retval = WriteConfigBlock( usb, AIOUSBDeviceGetADCConfigBlock( deviceDesc ));
                  
                  if (retval != AIOUSB_SUCCESS)
                      goto free_AIOUSB_ADC_InternalCal;
                  
                  /*
                   * we have good ground and reference readings;
                   * calculate table that makes ground reading
                   * equal to 0.0V and reference reading equal to
                   * 9.933939V; in order to compensate for an
                   * approximate 4.3 mV voltage drop across the
                   * primary MUX, we increase our target reference
                   * value by the same amount, yielding a new
                   * target of 9.933939V + 4.3 mV = 9.938239V =
                   * 65130.249 counts in unipolar mode
                   */

                  DoLoadCalTable(calTable, DeviceIndex, groundCounts, referenceCounts);
                  
                  AIOUSB_Lock();
                  ADCConfigBlockSetTriggerMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), 0);
                  ADCConfigBlockSetAllGainCodeAndDiffMode(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), AD_GAIN_CODE_0_10V, AIOUSB_FALSE);
                  ADCConfigBlockSetScanRange(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), 0, 1);
                  ADCConfigBlockSetOversample(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), 0);
                  
                  rangeValue = 0;
                  ADCConfigBlockSetRangeSingle(AIOUSBDeviceGetADCConfigBlock( deviceDesc ), rangeChannel, rangeValue);

              }
          }
    } else {
        /*
         * create default (1:1) calibration table; that is, each
         * output word equals the input word
         */
        int index;
        for(index = 0; index < CAL_TABLE_WORDS; index++)
            calTable[ index ] = ( unsigned short )index;
    }

    if(retval == AIOUSB_SUCCESS) {
      /*
       * optionally return calibration table to caller
       */
          if(returnCalTable != 0)
              memcpy(returnCalTable, calTable, CAL_TABLE_WORDS * sizeof(unsigned short));

          /*
           * optionally save calibration table to a file
           */
          if ( saveFileName != 0 ) {
              FILE *const calFile = fopen(saveFileName, "w");
              if(calFile != NULL) {
                  const size_t wordsWritten = fwrite(calTable, sizeof(unsigned short), CAL_TABLE_WORDS, calFile);
                  fclose(calFile);
                  if(wordsWritten != ( size_t )CAL_TABLE_WORDS) {
                      remove(saveFileName);             // file is likely corrupt or incomplete
                      retval = -AIOUSB_ERROR_FILE_NOT_FOUND;
                  }
              } else
                  retval = -AIOUSB_ERROR_FILE_NOT_FOUND;
          }

          /*
           * finally, send calibration table to device
           */
          retval = AIOUSB_ADC_SetCalTable(DeviceIndex, calTable);
      }


free_AIOUSB_ADC_InternalCal:
    free(calTable);
 out_AIOUSB_ADC_InternalCal:
    AIOUSB_UnLock();
    return retval;
}
/*----------------------------------------------------------------------------*/
void AIOUSB_SetRegister(ADCConfigBlock *cb, unsigned int Register, unsigned char value)
{
    AIOUSB_Lock();
    cb->registers[Register] = value;
    AIOUSB_UnLock();
}
/*----------------------------------------------------------------------------*/
unsigned char AIOUSB_GetRegister(ADCConfigBlock *cb, unsigned int Register)
{
    unsigned char tmpval;

    AIOUSB_Lock();
    tmpval = cb->registers[Register];
    AIOUSB_UnLock();
    return tmpval;
}
/*
 * we have to lock some of these functions because they access the device table; we don't
 * have to lock functions that don't access the device table
 */
/*----------------------------------------------------------------------------*/
void AIOUSB_SetAllGainCodeAndDiffMode(ADCConfigBlock *config, unsigned gainCode, AIOUSB_BOOL differentialMode)
{
    assert(config != 0);
    if(
        config != 0 &&
        config->device != 0 &&
        config->size != 0 &&
        gainCode >= FIRST_ENUM(ADGainCode) &&
        gainCode <= LAST_ENUM(ADGainCode)
        ) {
          if(differentialMode)
              gainCode |= AD_DIFFERENTIAL_MODE;
          unsigned channel;
          for(channel = 0; channel < AD_NUM_GAIN_CODE_REGISTERS; channel++)
              config->registers[ AD_CONFIG_GAIN_CODE + channel ] = gainCode;
      }
}

/*----------------------------------------------------------------------------*/
unsigned AIOUSB_GetGainCode(const ADCConfigBlock *config, unsigned channel)
{
    assert(config != 0);
    unsigned gainCode = FIRST_ENUM(ADGainCode);             // return reasonable value on error
    if( config != 0 && config->device != 0 &&   config->size != 0 ) { 
        const AIOUSBDevice *const deviceDesc = ( AIOUSBDevice* )config->device;
        if(channel < AD_MAX_CHANNELS && channel < deviceDesc->ADCMUXChannels) {
            assert(deviceDesc->ADCChannelsPerGroup != 0);
            gainCode = (config->registers[ AD_CONFIG_GAIN_CODE + channel / deviceDesc->ADCChannelsPerGroup ]
                        & ( unsigned char )AD_GAIN_CODE_MASK
                        );
        }
    }
    return gainCode;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief
 * @param config
 * @param channel
 * @param gainCode
 */
void AIOUSB_SetGainCode(ADCConfigBlock *config, unsigned channel, unsigned gainCode)
{
    assert(config != 0);
    if(
        config != 0 &&
        config->device != 0 &&
        config->size != 0 &&
        VALID_ENUM(ADGainCode, gainCode) &&
        AIOUSB_Lock()
        ) {
          const AIOUSBDevice *const deviceDesc = ( AIOUSBDevice* )config->device;
          if(channel < AD_MAX_CHANNELS && channel < deviceDesc->ADCMUXChannels) {
                assert(deviceDesc->ADCChannelsPerGroup != 0);
                const int reg = AD_CONFIG_GAIN_CODE + channel / deviceDesc->ADCChannelsPerGroup;
                assert(reg < AD_NUM_GAIN_CODE_REGISTERS);
                config->registers[ reg ]
                    = (config->registers[ reg ] & ~( unsigned char )AD_GAIN_CODE_MASK)
                      | ( unsigned char )(gainCode & AD_GAIN_CODE_MASK);
            }
          AIOUSB_UnLock();
      }
}

/*----------------------------------------------------------------------------*/
/**
 * @param config
 * @param channel
 * @return
 */
AIOUSB_BOOL AIOUSB_IsDifferentialMode(const ADCConfigBlock *config, unsigned channel)
{
    assert(config != 0);
    AIOUSB_BOOL differentialMode = AIOUSB_FALSE;
    if( config->device != 0 &&
        config->size != 0 &&
        AIOUSB_Lock()
        ) {
          const AIOUSBDevice *const deviceDesc = ( AIOUSBDevice* )config->device;
          if(
              channel < AD_MAX_CHANNELS &&
              channel < deviceDesc->ADCMUXChannels
              ) {
                assert(deviceDesc->ADCChannelsPerGroup != 0);
                differentialMode
                    = (
                    (
                        config->registers[ AD_CONFIG_GAIN_CODE + channel / deviceDesc->ADCChannelsPerGroup ]
                        & ( unsigned char )AD_DIFFERENTIAL_MODE
                    ) != 0
                    )
                      ? AIOUSB_TRUE
                      : AIOUSB_FALSE;
            }
          AIOUSB_UnLock();
      }
    return differentialMode;
}

/*----------------------------------------------------------------------------*/
/**
 * @param config
 * @param channel
 * @param differentialMode
 */
AIORET_TYPE AIOUSB_SetDifferentialMode(ADCConfigBlock *config, unsigned channel, AIOUSB_BOOL differentialMode)
{
    return ADCConfigBlockSetDifferentialMode( config, channel, differentialMode );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSB_GetCalMode( ADCConfigBlock *config)
{
    return ADCConfigBlockGetCalMode( config );
}

/*----------------------------------------------------------------------------*/
/**
 * @param config
 * @param calMode
 */
AIORET_TYPE AIOUSB_SetCalMode(ADCConfigBlock *config, unsigned calMode)
{
#ifdef __cplusplus
    return ADCConfigBlockSetCalMode( config, static_cast<ADCalMode>(calMode) );
#else 
    return ADCConfigBlockSetCalMode( config, calMode );
#endif
}

/*----------------------------------------------------------------------------*/
/**
 * @param config
 * @return
 */
AIORET_TYPE AIOUSB_GetTriggerMode(const ADCConfigBlock *config)
{
    return ADCConfigBlockGetTriggerMode( config );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSB_SetTriggerMode(ADCConfigBlock *config, unsigned triggerMode)
{
    return ADCConfigBlockSetTriggerMode( config , triggerMode );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSB_GetStartChannel(const ADCConfigBlock *config)
{
    return ADCConfigBlockGetStartChannel( config );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSB_GetEndChannel(const ADCConfigBlock *config)
{
    return ADCConfigBlockGetEndChannel( config );
}

/*----------------------------------------------------------------------------*/
/**
 * @param config
 * @param startChannel
 * @param endChannel
 */
AIORET_TYPE AIOUSB_SetScanRange(ADCConfigBlock *config, unsigned startChannel, unsigned endChannel)
{
    return ADCConfigBlockSetScanRange( config, startChannel, endChannel);
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSB_GetOversample(const ADCConfigBlock *config)
{
    return ADCConfigBlockGetOversample( config );
}

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSB_SetOversample(ADCConfigBlock *config, unsigned overSample)
{
    return ADCConfigBlockSetOversample( config, overSample );
}

/*----------------------------------------------------------------------------*/
static int CompareVoltage(const void *p1, const void *p2)
{
    assert(p1 != 0 &&
           p2 != 0);
    const double voltage1 = *( double* )p1, voltage2 = *( double* )p2;
    if(voltage1 < voltage2)
        return -1;
    else if(voltage1 > voltage2)
        return 1;
    else
        return 0;
}       // CompareVoltage()

/*----------------------------------------------------------------------------*/
AIORET_TYPE AIOUSB_ADC_ExternalCal(
                                     unsigned long DeviceIndex,
                                     const double points[],
                                     int numPoints,
                                     unsigned short returnCalTable[],
                                     const char *saveFileName
                                     )
{
    if(
        points == 0 ||
        numPoints < 2 ||
        numPoints > CAL_TABLE_WORDS
        )
        return AIOUSB_ERROR_INVALID_PARAMETER;

    const int INPUT_COLUMNS = 2, COLUMN_VOLTS = 0, COLUMN_COUNTS = 1;
    int index;
    for(index = 0; index < numPoints; index++) {
          if(
              points[ index * INPUT_COLUMNS + COLUMN_COUNTS ] < 0 ||
              points[ index * INPUT_COLUMNS + COLUMN_COUNTS ] > AI_16_MAX_COUNTS
              ) {
#if defined(DEBUG_EXT_CAL)
                printf("Error: invalid count value at point (%0.3f,%0.3f)\n",
                       points[ index * INPUT_COLUMNS + COLUMN_VOLTS ],
                       points[ index * INPUT_COLUMNS + COLUMN_COUNTS ]);
#endif
                return AIOUSB_ERROR_INVALID_PARAMETER;
            }
      }

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

    /**
     * @note
     * @verbatim
     * sort table into ascending order by input voltage; then verify that both the
     * input voltages and the measured counts are unique and uniformly increasing;
     * since the user's points[] array is declared to be 'const' we need to allocate
     * a working table that we can sort; in addition, we want to allocate space for
     * a slope and offset between each pair of points; so while points[] is like a
     * table with numPoints rows and two columns (input voltage, measured counts),
     * the working table effectively has the same number of rows, but four columns
     * (input voltage, measured counts, slope, offset)
     *
     *       points[] format:
     *       +-----------------+       +-----------------+
     *   [0] |  input voltage  |   [1] | measured counts |
     *       |=================|       |=================|
     *   [2] |  input voltage  |   [3] | measured counts |
     *       |=================|       |=================|
     *                            ...
     *       |=================|       |=================|
     * [n-2] |  input voltage  | [n-1] | measured counts |
     *       +-----------------+       +-----------------+
     * 'n' is not numPoints, but numPoints*2
     * @endverbatim
     */
    const int WORKING_COLUMNS = 4, COLUMN_SLOPE = 2, COLUMN_OFFSET = 3;
    double *const workingPoints = ( double* )malloc(numPoints * WORKING_COLUMNS * sizeof(double));
    assert(workingPoints != 0);
    if(workingPoints != 0) {
      /*
       * copy user's table to our working table and set slope and offset to valid values
       */
          for(index = 0; index < numPoints; index++) {
                workingPoints[ index * WORKING_COLUMNS + COLUMN_VOLTS ] = points[ index * INPUT_COLUMNS + COLUMN_VOLTS ];
                workingPoints[ index * WORKING_COLUMNS + COLUMN_COUNTS ] = points[ index * INPUT_COLUMNS + COLUMN_COUNTS ];
                workingPoints[ index * WORKING_COLUMNS + COLUMN_SLOPE ] = 1.0;
                workingPoints[ index * WORKING_COLUMNS + COLUMN_OFFSET ] = 0.0;
            }

          /*
           * sort working table in ascending order of input voltage
           */
          qsort(workingPoints, numPoints, WORKING_COLUMNS * sizeof(double), CompareVoltage);

          /*
           * verify that input voltages and measured counts are unique and ascending
           */
          for(index = 1 /* yes, 1 */; index < numPoints; index++) {
                if(
                    workingPoints[ index * WORKING_COLUMNS + COLUMN_VOLTS ] <=
                    workingPoints[ (index - 1) * WORKING_COLUMNS + COLUMN_VOLTS ] ||
                    workingPoints[ index * WORKING_COLUMNS + COLUMN_COUNTS ] <=
                    workingPoints[ (index - 1) * WORKING_COLUMNS + COLUMN_COUNTS ]
                    ) {
#if defined(DEBUG_EXT_CAL)
                      printf("Error: points (%0.3f,%0.3f) and (%0.3f,%0.3f) are not unique or not increasing\n",
                             workingPoints[ (index - 1) * WORKING_COLUMNS + COLUMN_VOLTS ],
                             workingPoints[ (index - 1) * WORKING_COLUMNS + COLUMN_COUNTS ],
                             workingPoints[ index * WORKING_COLUMNS + COLUMN_VOLTS ],
                             workingPoints[ index * WORKING_COLUMNS + COLUMN_COUNTS ]);
#endif
                      result = AIOUSB_ERROR_INVALID_PARAMETER;
                      break;
                  }
            }

          /**
           * @note if table of calibration points looks good, then proceed to calculate slopes and
           * offsets of line segments between points; we verified that no two points in the
           * table are equal, so we should not get any division by zero errors
           */
          if(result == AIOUSB_SUCCESS) {
            /**
             * @note
             * @verbatim the calibration table really only applies to one range if precision is our
             * objective; therefore, we assume that all the channels are configured for the
             * same range during calibration mode, and that the user is still using the same
             * range now as when they collected the calibration data points; if all these
             * assumptions are correct, then we can use the range setting for channel 0
             *
             * the calculations are based on the following model:
             *   mcounts = icounts x slope + offset
             * where,
             *   mcounts is the measured counts (reported by an uncalibrated A/D)
             *   icounts is the input counts from an external voltage source
             *   slope is the gain error inherent in the A/D and associated circuitry
             *   offset is the offset error inherent in the A/D and associated circuitry
             * to reverse the effect of these slope and offset errors, we use this equation:
             *   ccounts = ( mcounts – offset ) / slope
             * where,
             *   ccounts is the corrected counts
             * we calculate the slope and offset using these equations:
             *   slope = ( mcounts[s] – mcounts[z] ) / ( icounts[m] – icounts[z] )
             *   offset = mcounts[z] – icounts[z] x slope
             * where,
             *   [s] is the reading at "span" (the upper reference point)
             *   [z] is the reading at "zero" (the lower reference point)
             * in the simplest case, we would use merely two points to correct the entire voltage
             * range of the A/D; in such a simple case, the "zero" point would be a point near 0V,
             * and the "span" point would be a point near the top of the voltage range, such as 9.9V;
             * however, since this function is actually calculating a whole bunch of slope/offset
             * correction factors, one between each pair of points, "zero" refers to the lower of
             * two points, and "span" refers to the higher of the two points
             * @endverbatim
             */
                for(index = 1 /* yes, 1 */; index < numPoints; index++) {
                      const double counts0 = AIOUSB_VoltsToCounts(DeviceIndex, 0,           /* channel */
                                                                  workingPoints[ (index - 1) * WORKING_COLUMNS + COLUMN_VOLTS ]),
                                   counts1 = AIOUSB_VoltsToCounts(DeviceIndex, 0,           /* channel */
                                                                  workingPoints[ index * WORKING_COLUMNS + COLUMN_VOLTS ]);
                      const double slope
                          = (
                          workingPoints[ index * WORKING_COLUMNS + COLUMN_COUNTS ]
                          - workingPoints[ (index - 1) * WORKING_COLUMNS + COLUMN_COUNTS ]
                          )
                            / (counts1 - counts0);
                      const double offset
                          = workingPoints[ (index - 1) * WORKING_COLUMNS + COLUMN_COUNTS ]
                            - (counts0 * slope);
                      if(
                          slope >= 0.1 &&
                          slope <= 10.0 &&
                          offset >= -1000.0 &&
                          offset <= 1000.0
                          ) {
                            workingPoints[ index * WORKING_COLUMNS + COLUMN_SLOPE ] = slope;
                            workingPoints[ index * WORKING_COLUMNS + COLUMN_OFFSET ] = offset;
                        }else {
#if defined(DEBUG_EXT_CAL)
                            printf("Error: slope of %0.3f or offset of %0.3f is outside the allowed limits\n", slope, offset);
#endif
                            result = AIOUSB_ERROR_INVALID_DATA;             // slopes and offsets are way off, abort
                            break;                                // from for()
                        }
                  }
            }

          if(result == AIOUSB_SUCCESS) {
#if defined(DEBUG_EXT_CAL)
                printf(
                    "External Calibration Points\n"
                    "     Input    Measured  Calculated  Calculated\n"
                    "     Volts      Counts       Slope      Offset\n"
                    );
                for(index = 0; index < numPoints; index++) {
                      printf("%10.3f  %10.3f  %10.3f  %10.3f\n",
                             workingPoints[ index * WORKING_COLUMNS + COLUMN_VOLTS ],
                             workingPoints[ index * WORKING_COLUMNS + COLUMN_COUNTS ],
                             workingPoints[ index * WORKING_COLUMNS + COLUMN_SLOPE ],
                             workingPoints[ index * WORKING_COLUMNS + COLUMN_OFFSET ]
                             );
                  }
#endif

/*
 * generate calibration table using the equation
 *   ccounts = ( mcounts – offset ) / slope
 * described above; each slope/offset pair in workingPoints[] describes the line
 * segment running between the _previous_ point and the current one; in addition,
 * the first row in workingPoints[] doesn't contain a valid slope/offset pair
 * because there is no previous point before the first one (!), so we stretch the
 * first line segment (between points 0 and 1) backward to the beginning of the A/D
 * count range; similarly, since the highest calibration point is probably not right
 * at the top of the A/D count range, we stretch the highest line segment (between
 * points n-2 and n-1) up to the top of the A/D count range
 */
                unsigned short *const calTable = ( unsigned short* )malloc(CAL_TABLE_WORDS * sizeof(unsigned short));
                if(calTable != 0) {
                      int measCounts = 0;                 // stretch first line segment to bottom of A/D count range
                      for(index = 1 /* yes, 1 */; index < numPoints; index++) {
                            const double slope = workingPoints[ index * WORKING_COLUMNS + COLUMN_SLOPE ],
                                         offset = workingPoints[ index * WORKING_COLUMNS + COLUMN_OFFSET ];
                            const int maxSegmentCounts
                                = (index == (numPoints - 1))
                                  ? (CAL_TABLE_WORDS - 1)                 // stretch last line segment to top of A/D count range
                                  : ( int )workingPoints[ index * WORKING_COLUMNS + COLUMN_COUNTS ];
                            for(; measCounts <= maxSegmentCounts; measCounts++) {
                                  int corrCounts = round((measCounts - offset) / slope);
                                  if(corrCounts < 0)
                                      corrCounts = 0;
                                  else if(corrCounts > AI_16_MAX_COUNTS)
                                      corrCounts = AI_16_MAX_COUNTS;
                                  calTable[ measCounts ] = corrCounts;
                              }
                        }

/*
 * optionally return calibration table to caller
 */
                      if(returnCalTable != 0)
                          memcpy(returnCalTable, calTable, CAL_TABLE_WORDS * sizeof(unsigned short));

/*
 * optionally save calibration table to a file
 */
                      if(saveFileName != 0) {
                            FILE *const calFile = fopen(saveFileName, "w");
                            if(calFile != NULL) {
                                  const size_t wordsWritten = fwrite(calTable, sizeof(unsigned short), CAL_TABLE_WORDS, calFile);
                                  fclose(calFile);
                                  if(wordsWritten != ( size_t )CAL_TABLE_WORDS) {
                                        remove(saveFileName);                 // file is likely corrupt or incomplete
                                        result = AIOUSB_ERROR_FILE_NOT_FOUND;
                                    }
                              }else
                                result = AIOUSB_ERROR_FILE_NOT_FOUND;
                        }

                      /*
                       * finally, send calibration table to device
                       */
                      result = AIOUSB_ADC_SetCalTable(DeviceIndex, calTable);

                      free(calTable);
                  }else
                    result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;
            }

          free(workingPoints);
      }else
        result = AIOUSB_ERROR_NOT_ENOUGH_MEMORY;

    return result;
}


#ifdef __cplusplus
}
#endif


