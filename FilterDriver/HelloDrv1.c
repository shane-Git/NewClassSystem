/*++
Module Name:
    hellodrv.c

Abstract:
	��ʾһ������򵥵��ں�����������ͨ��debug Log�������һ����Ϣ
	
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
