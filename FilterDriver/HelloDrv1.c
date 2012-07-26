/*++
Module Name:
    hellodrv.c

Abstract:
	演示一个最最简单的内核驱动程序，它通过debug Log向外输出一段信息
	
--*/

#include <ntddk.h>





DRIVER_INITIALIZE DriverEntry;

#pragma alloc_text (INIT, DriverEntry)

NTSTATUS
DriverEntry(
    __in PDRIVER_OBJECT  DriverObject,
    __in PUNICODE_STRING RegistryPath
    )
{
    NTSTATUS  status = STATUS_UNSUCCESSFUL;

    UNREFERENCED_PARAMETER(RegistryPath);
    UNREFERENCED_PARAMETER(DriverObject);

    DbgPrintEx(DPFLTR_DEFAULT_ID,DPFLTR_ERROR_LEVEL,"Hello Driver!\n");

    return status;
}
