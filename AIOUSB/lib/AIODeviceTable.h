#ifndef _AIO_DEVICE_TABLE_H
#define _AIO_DEVICE_TABLE_H

#include "AIOTypes.h"
#include "AIOUSBDevice.h"
#include "AIOUSB_Core.h"
#include <string.h>

#include "libusb.h"
#include <stdlib.h>

#ifdef __aiousb_cplusplus
namespace AIOUSB
{
#endif

PUBLIC_EXTERN AIORESULT AIODeviceTableAddDeviceToDeviceTable( int *numAccesDevices, unsigned long productID ) ;
PUBLIC_EXTERN AIORESULT AIODeviceTableAddDeviceToDeviceTableWithUSBDevice( int *numAccesDevices, unsigned long productID ,libusb_device *usb_dev );
PUBLIC_EXTERN void AIODeviceTablePopulateTable(void);
PUBLIC_EXTERN void AIODeviceTablePopulateTableTest(unsigned long *products, int length );
PUBLIC_EXTERN AIORESULT AIODeviceTableClearDevices( void );
PUBLIC_EXTERN AIOUSBDevice *AIODeviceTableGetDeviceAtIndex( unsigned long index , AIORESULT *result );

PUBLIC_EXTERN unsigned long QueryDeviceInfo( unsigned long DeviceIndex, unsigned long *pPID, unsigned long *pNameSize, 
                                             char *pName, unsigned long *pDIOBytes, unsigned long *pCounters );

PUBLIC_EXTERN char *GetSafeDeviceName( unsigned long DeviceIndex );
PUBLIC_EXTERN char *ProductIDToName( unsigned int productID );

PUBLIC_EXTERN AIORESULT AIOUSB_Init(void);
PUBLIC_EXTERN AIORESULT AIOUSB_EnsureOpen(unsigned long DeviceIndex);
PUBLIC_EXTERN AIOUSB_BOOL AIOUSB_IsInit();
PUBLIC_EXTERN void AIOUSB_Exit();
PUBLIC_EXTERN AIORESULT AIOUSB_Reset( unsigned long DeviceIndex );
PUBLIC_EXTERN void AIODeviceTableInit(void);


PUBLIC_EXTERN void CloseAllDevices(void);
PUBLIC_EXTERN AIORESULT AIOUSB_GetAllDevices();

PUBLIC_EXTERN unsigned long AIOUSB_INIT_PATTERN;

#ifdef __aiousb_cplusplus
}
#endif

#endif
