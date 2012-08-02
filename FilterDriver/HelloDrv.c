/*++
Module Name:
    hellodrv.c

Abstract:
	演示一个最最简单的内核驱动程序，它通过debug Log向外输出一段信息
	
--*/

#include <ntddk.h>
#include <ntddkbd.h>

#define KEY_UP        1
#define KEY_DOWN      0  

#define LCONTROL      ((USHORT)0x1D)
#define CAPS_LOCK      ((USHORT)0x3A)  


typedef struct DEVICE_EXTENSION {
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

PDEVICE_OBJECT HookDeviceObject;
PDEVICE_OBJECT kbdDevice;

DRIVER_INITIALIZE DriverEntry;
DRIVER_DISPATCH KSnifferDispatchRead;
DRIVER_DISPATCH KSnifferDispatchGeneral;
DRIVER_UNLOAD KSnifferDriverUnload;
DRIVER_ADD_DEVICE KSnifferAddDevice;
IO_COMPLETION_ROUTINE KSnifferReadComplete;

#pragma alloc_text (INIT, DriverEntry)

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


NTSTATUS KSnifferDispatchRead(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp);
NTSTATUS KSnifferReadComplete(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp,IN PVOID Context);
NTSTATUS KSnifferDispatchGeneral(IN PDEVICE_OBJECT DeviceObject,IN PIRP Irp );
NTSTATUS DispatchPnp(PDEVICE_OBJECT fido, PIRP Irp);
NTSTATUS DispatchPower(PDEVICE_OBJECT fido, PIRP Irp);
VOID KSnifferDriverUnload(IN PDRIVER_OBJECT DeviceObject);            
NTSTATUS KSnifferAddDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pPhysDevObj);   


NTSTATUS UsageNotificationCompletionRoutine(PDEVICE_OBJECT fido, PIRP Irp, PDEVICE_EXTENSION pdx);
NTSTATUS StartDeviceCompletionRoutine(PDEVICE_OBJECT fido, PIRP Irp, PDEVICE_EXTENSION pdx);
VOID RemoveDevice(IN PDEVICE_OBJECT fido);

NTSTATUS DriverEntry(IN PDRIVER_OBJECT DriverObject,IN PUNICODE_STRING RegistryPath)        
{
	
	STRING ntNameString;
    UNICODE_STRING ntUnicodeString;
	UNICODE_STRING DriverName;
	UNICODE_STRING DevName;
    NTSTATUS status;
	UCHAR ucCnt = 0;
	PDRIVER_OBJECT AudioDriver = NULL;
	PDEVICE_OBJECT DeviceObject = NULL;
	
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Device Add In\n");
    RtlInitAnsiString(&ntNameString,"\\Device\\KeyboardClass0");
    RtlAnsiStringToUnicodeString(&ntUnicodeString,&ntNameString,TRUE);
	RtlInitUnicodeString(&DevName,L"\\Device\\KBfilter0");
	RtlInitUnicodeString(&DriverName,L"\\Driver\\usbaudio");

	
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Enter DriverEntry \n");
	
	for (ucCnt = 0; ucCnt < IRP_MJ_MAXIMUM_FUNCTION; ucCnt++)
	{
		DriverObject->MajorFunction[ucCnt] = KSnifferDispatchGeneral;
	}
	DriverObject->MajorFunction[IRP_MJ_POWER] = DispatchPower;
	DriverObject->MajorFunction[IRP_MJ_PNP] = DispatchPnp;
    DriverObject->MajorFunction[IRP_MJ_READ] = KSnifferDispatchRead;
	DriverObject->DriverUnload = KSnifferDriverUnload;
	DriverObject->DriverExtension->AddDevice = KSnifferAddDevice;
	
	
	ObReferenceObjectByName( &DriverName,
                           OBJ_CASE_INSENSITIVE,
                           NULL,
                           0,
                           *IoDriverObjectType,
                           KernelMode,
                           NULL,
                           &AudioDriver );
	if ( AudioDriver == NULL )
	{
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Not found USB Keyboard Device hidusb!\n" );
		return STATUS_UNSUCCESSFUL;
	}
	else
	{
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Get it found USB Keyboard Device hidusb!\n" );
		DeviceObject = AudioDriver -> DeviceObject;
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"hidusb!%x\n",DeviceObject);
	}

    status = IoCreateDevice(DriverObject,         
                 2*sizeof(PDEVICE_OBJECT),
                 &DevName,
                 FILE_DEVICE_UNKNOWN,
                 0,
                 FALSE,
                 &HookDeviceObject);            //建立一键盘类设备
	if(!NT_SUCCESS(status)) 
	{
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Init Error\n");
		RtlFreeUnicodeString(&ntUnicodeString);
		return STATUS_SUCCESS;
	}
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Successfully Create\n");
    HookDeviceObject->Flags |= DO_BUFFERED_IO;
    IoAttachDeviceToDeviceStack(HookDeviceObject,DeviceObject);      //连接我们的过滤设备到\\Device\\KeyboardClass0设备上
    if(!NT_SUCCESS(status)) 
	{
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Connect with keyboard failed!\n");
		IoDeleteDevice(HookDeviceObject);
		RtlFreeUnicodeString(&ntUnicodeString);
		return STATUS_SUCCESS;
	}
	RtlFreeUnicodeString( &ntUnicodeString );

	return STATUS_SUCCESS;
}


NTSTATUS KSnifferDispatchRead( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp )        //有按键按下时执行
{
	PIO_STACK_LOCATION currentIrpStack = IoGetCurrentIrpStackLocation(Irp);        //获取当前的IRP包
	PIO_STACK_LOCATION nextIrpStack    = IoGetNextIrpStackLocation(Irp);
//	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"This is Read\n");
	*nextIrpStack = *currentIrpStack;
	IoSetCompletionRoutine( Irp, KSnifferReadComplete, DeviceObject, TRUE, TRUE, TRUE );     //调用完成例程
	return IoCallDriver( kbdDevice, Irp );
}

NTSTATUS KSnifferReadComplete( IN PDEVICE_OBJECT DeviceObject, IN PIRP Irp, IN PVOID Context )
{
	PIO_STACK_LOCATION IrpSp;
	PKEYBOARD_INPUT_DATA KeyData;
	UNREFERENCED_PARAMETER(DeviceObject);
	UNREFERENCED_PARAMETER(Context);
	IrpSp = IoGetCurrentIrpStackLocation( Irp );
	if( NT_SUCCESS( Irp->IoStatus.Status ) ) 
    {
		KeyData = Irp->AssociatedIrp.SystemBuffer;
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"ScanCode: %x ", KeyData->MakeCode );
		DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"%s\n", KeyData->Flags ? "Up" : "Down" );      //输出按键的扫描码
		if( KeyData->MakeCode == CAPS_LOCK) 
		{
			KeyData->MakeCode = LCONTROL;                  //修改按键
		}  
	}
	if( Irp->PendingReturned ) 
	{
		IoMarkIrpPending( Irp );
	}
	return Irp->IoStatus.Status;
}

NTSTATUS KSnifferDispatchGeneral(                //通用事件处理例程
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

VOID KSnifferDriverUnload(IN PDRIVER_OBJECT DeviceObject)
{
	UNREFERENCED_PARAMETER(DeviceObject);
	IoDetachDevice(kbdDevice);
	IoDeleteDevice(HookDeviceObject);
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Exit the filter");
}

NTSTATUS KSnifferAddDevice(IN PDRIVER_OBJECT DeviceObject,IN PDEVICE_OBJECT FunctionalDeviceObject)
{
	DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Successfully connected to keyboard device\n");
	FunctionalDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
	return STATUS_SUCCESS;
}

NTSTATUS DispatchPnp(PDEVICE_OBJECT fido, PIRP Irp)
{
	PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
	ULONG fcn = stack->MinorFunction;
	NTSTATUS status;
	PDEVICE_EXTENSION pdx = (PDEVICE_EXTENSION) fido->DeviceExtension;
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