/*++
Module Name:
    hellodrv.c

Abstract:
	演示一个最最简单的内核驱动程序，它通过debug Log向外输出一段信息
	
--*/

#include <ntddk.h>
#include <ntddkbd.h>
#include <Usbioctl.h>
#include <Usb.h>

DRIVER_INITIALIZE DriverEntry;
DRIVER_UNLOAD DriverUnload;

typedef struct DEVICE_EXTENSION
{
	PDEVICE_OBJECT DeviceObject;   // device object this extension belongs to
	PDEVICE_OBJECT LowerDeviceObject;  // next lower driver in same stack
	PDEVICE_OBJECT Pdo;      // the PDO
	IO_REMOVE_LOCK RemoveLock;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

 
NTSTATUS CompleteRequest(IN PIRP Irp, IN NTSTATUS status, IN ULONG_PTR info)
{       // CompleteRequest
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}

ULONG GetDeviceTypeToUse(PDEVICE_OBJECT pdo)
{							// GetDeviceTypeToUse
	PDEVICE_OBJECT ldo;
	ULONG devtype;
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"This function is used to get device type\n");
	ldo = IoGetAttachedDeviceReference(pdo);
	if (!ldo)
		return FILE_DEVICE_UNKNOWN;
	devtype = ldo->DeviceType;
	ObDereferenceObject(ldo);
	return devtype;
}

void DumpBuffer(unsigned char * buf, int len)
{
#define NB_BYTE 16 /* number of bytes displayed per line */

	char str[NB_BYTE*3 + 1];
	char * p;
	int i,j;
	char c;

	for (i=0;i<len;i+=NB_BYTE)
	{
		p = str;

		for (j=i;j<len&&j<i+NB_BYTE;j++)
		{
			c;

			*p++ = ' ';
			c = (buf[j] >> 4) & 0xf;
			*p++ = (c<10) ? c+'0' : c-10+'a';
			c = buf[j] & 0xf;
			*p++ = (c<10) ? c+'0' : c-10+'a';
		}
		*p = 0;

		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"\t\t%04x:%s\n",i,str);
	}
}


extern POBJECT_TYPE *IoDriverObjectType;
extern NTKERNELAPI
NTSTATUS
ObReferenceObjectByName(
    IN PUNICODE_STRING ObjectName,
    IN ULONG Attributes,
    IN PACCESS_STATE PassedAccessState OPTIONAL,
    IN ACCESS_MASK DesiredAccess OPTIONAL,
    IN POBJECT_TYPE ObjectType,
    IN KPROCESSOR_MODE AccessMode,
    IN OUT PVOID ParseContext OPTIONAL,
    OUT PVOID *Object
    );


NTSTATUS DispatchAny(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp );
NTSTATUS DispatchPnp(PDEVICE_OBJECT fido, PIRP Irp);
NTSTATUS DispatchPower(PDEVICE_OBJECT fido, PIRP Irp);
VOID DriverUnload(IN PDRIVER_OBJECT DeviceObject);            
NTSTATUS AddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pPhysDevObj);   

NTSTATUS DispatchSpecial(IN PDEVICE_OBJECT fido,IN PIRP Irp);


NTSTATUS UsageNotificationCompletionRoutine(PDEVICE_OBJECT fido, PIRP Irp, PDEVICE_EXTENSION pdx);
NTSTATUS StartDeviceCompletionRoutine(PDEVICE_OBJECT fido, PIRP Irp, PDEVICE_EXTENSION pdx);
VOID RemoveDevice(IN PDEVICE_OBJECT fido);

#pragma alloc_text (INIT, DriverEntry)

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,IN PUNICODE_STRING RegistryPath)        
{
	UCHAR ucCnt = 0;
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Enter DriverEntry \n");
	DriverObject->DriverUnload = DriverUnload;
	DriverObject->DriverExtension->AddDevice = AddDevice;
	for (ucCnt = 0; ucCnt < IRP_MJ_MAXIMUM_FUNCTION; ucCnt++)
	{
		DriverObject->MajorFunction[ucCnt] = DispatchAny;
	}
	DriverObject->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
	DriverObject->MajorFunction[IRP_MJ_POWER] = DispatchPower;
	DriverObject->MajorFunction[IRP_MJ_INTERNAL_DEVICE_CONTROL] = DispatchSpecial;
	return STATUS_SUCCESS;
}

NTSTATUS DispatchAny(
    IN PDEVICE_OBJECT fido,
    IN PIRP          Irp )
{
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fido->DeviceExtension;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status;
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"This is General\n");
	status = IoAcquireRemoveLock(&pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(status))
		return CompleteRequest(Irp, status, 0);
	IoSkipCurrentIrpStackLocation(Irp);
	status = IoCallDriver(pdx->LowerDeviceObject, Irp);
	IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
	return status;
}

NTSTATUS DispatchSpecial(
    IN PDEVICE_OBJECT fido,
    IN PIRP          Irp )
{
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fido->DeviceExtension;
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	NTSTATUS status;
	PURB pUrb;
	USHORT cFounctionCode;
	USHORT cLength;
	USBD_STATUS lUsbdStatus;
	struct _URB_ISOCH_TRANSFER * pIsochTransfer;
	ULONG dwControlCode = stack->Parameters.DeviceIoControl.IoControlCode;
	if(dwControlCode == IOCTL_INTERNAL_USB_SUBMIT_URB)
	{
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"A URB was submitted\n");
		pUrb = (PURB) stack->Parameters.Others.Argument1;
		if(pUrb == NULL)
		{
			DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Not Found URB\n");
		}
		else
		{
			cFounctionCode = pUrb->UrbHeader.Function;
			cLength = pUrb->UrbHeader.Length;
			lUsbdStatus = pUrb->UrbHeader.Status;
			DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Found URB,Function:%d,Length:%d,status:0x%X\n",cFounctionCode,cLength,lUsbdStatus);
			if(cFounctionCode == URB_FUNCTION_ISOCH_TRANSFER)
			{
				pIsochTransfer = (struct _URB_ISOCH_TRANSFER *) pUrb;
				DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"ISOCH_TRANSFER,length:%u,Buffer:%X,StartFrame:%u,NumberOfPkt:%u\n"
				,pIsochTransfer->TransferBufferLength,pIsochTransfer->TransferBuffer,pIsochTransfer->StartFrame,pIsochTransfer->NumberOfPackets);
				DumpBuffer(pIsochTransfer->TransferBuffer,pIsochTransfer->TransferBufferLength);
			}
		}
	}
	else
	{
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"This is Special\n");
	}
	status = IoAcquireRemoveLock(&pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(status))
		return CompleteRequest(Irp, status, 0);
	IoSkipCurrentIrpStackLocation(Irp);
	status = IoCallDriver(pdx->LowerDeviceObject, Irp);
	IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
	return status;
}

VOID DriverUnload(IN PDRIVER_OBJECT DeviceObject)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Exit the filter\n");
}

NTSTATUS AddDevice(IN PDRIVER_OBJECT DriverObject,IN PDEVICE_OBJECT pdo)
{
	NTSTATUS status;
	PDEVICE_OBJECT fido;
	PDEVICE_EXTENSION pdx;
	PDEVICE_OBJECT fdo;
	
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Successfully connected Driver:%X;Device:%X\n",DriverObject,pdo);
	
	status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), NULL,
		FILE_DEVICE_UNKNOWN, 0, FALSE, &fido);
	
	if (!NT_SUCCESS(status))
	{						// can't create device object
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL," - IoCreateDevice failed - %X\n", status);
		return status;
	}						// can't create device object
	pdx = (PDEVICE_EXTENSION) fido->DeviceExtension;

	do
	{	// finish initialization
		IoInitializeRemoveLock(&pdx->RemoveLock, 0, 0, 0);
		pdx->DeviceObject = fido;
		pdx->Pdo = pdo;
		//将过滤驱动附加在底层驱动之上
		fdo = IoAttachDeviceToDeviceStack(fido, pdo);
		if (!fdo)
		{					// can't attach								 
			DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"DRIVERNAME  - IoAttachDeviceToDeviceStack failed\n");
			status = STATUS_DEVICE_REMOVED;
			break;
		}					// can't attach
		//记录底层驱动
		pdx->LowerDeviceObject = fdo;
		//由于不知道底层驱动是直接IO还是BufferIO，因此将标志都置上
		fido->Flags |= fdo->Flags & (DO_DIRECT_IO | DO_BUFFERED_IO | DO_POWER_PAGABLE);
		// Clear the "initializing" flag so that we can get IRPs
		fido->Flags &= ~DO_DEVICE_INITIALIZING;
	}	while (FALSE);					// finish initialization

	if (!NT_SUCCESS(status))
	{					// need to cleanup
		if (pdx->LowerDeviceObject)
			IoDetachDevice(pdx->LowerDeviceObject);
		IoDeleteDevice(fido);
	}					// need to cleanup

	return status;
}

NTSTATUS DispatchPnp(PDEVICE_OBJECT fido, PIRP Irp)
{
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG fcn = stack->MinorFunction;
	NTSTATUS status;
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fido->DeviceExtension;
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"PNP\n");
	status = IoAcquireRemoveLock(&pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(status))
		return CompleteRequest(Irp, status, 0);
		if (fcn == IRP_MN_DEVICE_USAGE_NOTIFICATION)
	{						// usage notification
		if (!fido->AttachedDevice || (fido->AttachedDevice->Flags & DO_POWER_PAGABLE))
			fido->Flags |= DO_POWER_PAGABLE;
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(Irp, (PIO_COMPLETION_ROUTINE) UsageNotificationCompletionRoutine,
			(PVOID) pdx, TRUE, TRUE, TRUE);
		return IoCallDriver(pdx->LowerDeviceObject, Irp);
	}						// usage notification

	// Handle start device specially in order to correctly inherit
	// FILE_REMOVABLE_MEDIA
	if (fcn == IRP_MN_START_DEVICE)
	{						// device start
		IoCopyCurrentIrpStackLocationToNext(Irp);
		IoSetCompletionRoutine(Irp, (PIO_COMPLETION_ROUTINE) StartDeviceCompletionRoutine,
			(PVOID) pdx, TRUE, TRUE, TRUE);
		return IoCallDriver(pdx->LowerDeviceObject, Irp);
	}						// device start

	// Handle remove device specially in order to cleanup device stack
	if (fcn == IRP_MN_REMOVE_DEVICE)
	{						// remove device
		IoSkipCurrentIrpStackLocation(Irp);
		status = IoCallDriver(pdx->LowerDeviceObject, Irp);
		IoReleaseRemoveLockAndWait(&pdx->RemoveLock, Irp);
		RemoveDevice(fido);
		return status;
	}						// remove device

	// Simply forward any other type of PnP request
	IoSkipCurrentIrpStackLocation(Irp);
	status = IoCallDriver(pdx->LowerDeviceObject, Irp);
	IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
	return status;
}

NTSTATUS DispatchPower(PDEVICE_OBJECT fido, PIRP Irp)
{
	PDEVICE_EXTENSION pdx;
	NTSTATUS status;
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Power\n");
	PoStartNextPowerIrp(Irp);
	pdx = (PDEVICE_EXTENSION) fido->DeviceExtension;
	status = IoAcquireRemoveLock(&pdx->RemoveLock, Irp);
	if (!NT_SUCCESS(status))
	{
		return CompleteRequest(Irp, status, 0);
	}
	IoSkipCurrentIrpStackLocation(Irp);
	status = PoCallDriver(pdx->LowerDeviceObject, Irp);
	IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
	return status;
}


NTSTATUS UsageNotificationCompletionRoutine(PDEVICE_OBJECT fido, PIRP Irp, PDEVICE_EXTENSION pdx)
{							// UsageNotificationCompletionRoutine
	if (Irp->PendingReturned)
		IoMarkIrpPending(Irp);
	// If lower driver cleared pageable flag, we must do the same
	if (!(pdx->LowerDeviceObject->Flags & DO_POWER_PAGABLE))
		fido->Flags &= ~DO_POWER_PAGABLE;
	IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
	return STATUS_SUCCESS;
}

NTSTATUS StartDeviceCompletionRoutine(PDEVICE_OBJECT fido, PIRP Irp, PDEVICE_EXTENSION pdx)
{							// StartDeviceCompletionRoutine
	if (Irp->PendingReturned)
		IoMarkIrpPending(Irp);
	// Inherit FILE_REMOVABLE_MEDIA flag from lower object. This is necessary
	// for a disk filter, but it isn't available until start-device time. Drivers
	// above us may examine the flag as part of their own start-device processing, too.
	if (pdx->LowerDeviceObject->Characteristics & FILE_REMOVABLE_MEDIA)
		fido->Characteristics |= FILE_REMOVABLE_MEDIA;
	IoReleaseRemoveLock(&pdx->RemoveLock, Irp);
	return STATUS_SUCCESS;
}

VOID RemoveDevice(IN PDEVICE_OBJECT fido)
{							// RemoveDevice
	PDEVICE_EXTENSION pdx;
	PAGED_CODE();
	pdx = (PDEVICE_EXTENSION) fido->DeviceExtension;
	if (pdx->LowerDeviceObject)
		IoDetachDevice(pdx->LowerDeviceObject);
	IoDeleteDevice(fido);
}