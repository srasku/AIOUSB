/**
 * @file   AIOUSB_CustomEEPROM.c
 * @author $Format: %an <%ae>$
 * @date   $Format: %ad$
 * @version $Format: %t$
 * @brief  General header files for EEProm functionality
 *
 */

#include "AIOUSB_CustomEEPROM.h"
#include "AIOUSB_Core.h"
#include "AIODeviceTable.h"

#ifdef __cplusplus
namespace AIOUSB {
#endif

/*----------------------------------------------------------------------------*/
/**
* EEPROM layout:
*   program code: 0x0000 -> EEPROM_CUSTOM_BASE_ADDRESS - 1
*   user space  : EEPROM_CUSTOM_BASE_ADDRESS -> EEPROM_CUSTOM_BASE_ADDRESS + EEPROM_CUSTOM_MAX_ADDRESS - 1
*                 (user space is addressed as 0 -> EEPROM_CUSTOM_MAX_ADDRESS - 1)
*/
unsigned long CustomEEPROMWrite(
                                unsigned long DeviceIndex,
                                unsigned long StartAddress,
                                unsigned long DataSize,
                                void *Data
                                )
{

    if(
        StartAddress > EEPROM_CUSTOM_MAX_ADDRESS ||
        (StartAddress + DataSize) > EEPROM_CUSTOM_MAX_ADDRESS + 1 ||
        Data == NULL
        )
        return AIOUSB_ERROR_INVALID_PARAMETER;

    if(!AIOUSB_Lock())
        return AIOUSB_ERROR_INVALID_MUTEX;

    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }

    libusb_device_handle *const deviceHandle = AIOUSB_GetDeviceHandle(DeviceIndex);
    if(deviceHandle != NULL) {
          const unsigned timeout = deviceDesc->commTimeout;
          AIOUSB_UnLock();                                                    // unlock while communicating with device
          const int bytesTransferred = libusb_control_transfer(deviceHandle,
                                                               USB_WRITE_TO_DEVICE, AUR_EEPROM_WRITE,
                                                               EEPROM_CUSTOM_BASE_ADDRESS + StartAddress, 0,
                                                               ( unsigned char* )Data, DataSize, timeout);
          if(bytesTransferred != ( int )DataSize)
              result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
      }else {
          result = AIOUSB_ERROR_DEVICE_NOT_CONNECTED;
          AIOUSB_UnLock();
      }

    return result;
}

/*----------------------------------------------------------------------------*/
/**
 * @brief
 * EEPROM layout:
 *   program code: 0x0000 -> EEPROM_CUSTOM_BASE_ADDRESS - 1
 *   user space  : EEPROM_CUSTOM_BASE_ADDRESS -> EEPROM_CUSTOM_BASE_ADDRESS + EEPROM_CUSTOM_MAX_ADDRESS - 1
 *                 (user space is addressed as 0 -> EEPROM_CUSTOM_MAX_ADDRESS - 1)
 */
unsigned long CustomEEPROMRead(
    unsigned long DeviceIndex,
    unsigned long StartAddress,
    unsigned long *DataSize,
    void *Data
    )
{

    if(
        StartAddress > EEPROM_CUSTOM_MAX_ADDRESS ||
        (StartAddress + *DataSize) > EEPROM_CUSTOM_MAX_ADDRESS + 1 ||
        Data == NULL
        )
        return AIOUSB_ERROR_INVALID_PARAMETER;

    if(!AIOUSB_Lock())
        return AIOUSB_ERROR_INVALID_MUTEX;
    AIORESULT result = AIOUSB_SUCCESS;
    AIOUSBDevice *deviceDesc = AIODeviceTableGetDeviceAtIndex( DeviceIndex, &result );
    if ( result != AIOUSB_SUCCESS ){
        AIOUSB_UnLock();
        return result;
    }


    libusb_device_handle *const deviceHandle = AIOUSB_GetDeviceHandle(DeviceIndex);
    if(deviceHandle != NULL) {
          const unsigned timeout = deviceDesc->commTimeout;
          AIOUSB_UnLock();                                                    // unlock while communicating with device
          const int bytesTransferred = libusb_control_transfer(deviceHandle,
                                                               USB_READ_FROM_DEVICE, AUR_EEPROM_READ,
                                                               EEPROM_CUSTOM_BASE_ADDRESS + StartAddress, 0,
                                                               ( unsigned char* )Data, *DataSize, timeout);
          if(bytesTransferred != ( int )*DataSize)
              result = LIBUSB_RESULT_TO_AIOUSB_RESULT(bytesTransferred);
      }else {
          result = AIOUSB_ERROR_DEVICE_NOT_CONNECTED;
          AIOUSB_UnLock();
      }

    return result;
}


#ifdef __cplusplus
}
#endif

