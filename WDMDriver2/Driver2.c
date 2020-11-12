#include <ntddk.h>
#include <wdf.h>
#include <ntstrsafe.h>
#include "NotifyClass.h"
#define DRIVER_NAME "WDMDriver2"
#define TRACKED_PROCESS_NAME "regedit.exe"
#define LOG_FILE_PATH L"\\??\\C:\\Driver2.log"
#define DELIMITER " : "
typedef PCHAR(*GET_PROCESS_IMAGE_NAME) (PEPROCESS Process);
GET_PROCESS_IMAGE_NAME GetProcessImageFileName;
DRIVER_INITIALIZE DriverEntry;
PDEVICE_OBJECT GlobalDeviceObject;
BOOLEAN IsCallbackCreated=FALSE;
LARGE_INTEGER Cookie;
ANSI_STRING TrackedProcessName;
UNICODE_STRING LogFileName;




extern NTSTATUS PsLookupProcessByProcessId(HANDLE ProcessId, PEPROCESS* Process);
void SendEvent(PKEVENT _event) {
	KeSetEvent(_event, 0, FALSE);
	KeClearEvent(_event);
}


NTSTATUS OpenFile(PUNICODE_STRING fileName, PHANDLE handle)
{
	OBJECT_ATTRIBUTES objectAttributes;
	IO_STATUS_BLOCK ioStatusBlock;
	InitializeObjectAttributes(&objectAttributes, fileName, OBJ_OPENIF, NULL, NULL);
	return ZwCreateFile(handle,FILE_APPEND_DATA,&objectAttributes,&ioStatusBlock,NULL,FILE_ATTRIBUTE_NORMAL,FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,FILE_OPEN_IF,FILE_NON_DIRECTORY_FILE,NULL,0);
}


void LogToFile(PANSI_STRING Operation) {
	HANDLE LogFileHandle;
	NTSTATUS Status;
	ANSI_STRING LogString;
	IO_STATUS_BLOCK IoStatusBlock;
	LARGE_INTEGER buf;
	char str[1000];
	buf.HighPart = -1;
	buf.LowPart = FILE_WRITE_TO_END_OF_FILE;
	Status = OpenFile(&LogFileName, &LogFileHandle);
	if (NT_SUCCESS(Status)) {
		strncat(str, DRIVER_NAME, 100);
		strncat(str, DELIMITER, 100);
		strncat(str, TRACKED_PROCESS_NAME, 100);
		strncat(str, DELIMITER, 100);
		strncat(str, Operation->Buffer, 100);
		strcat(str, "\n");
		RtlInitAnsiString(&LogString, str);
		ZwWriteFile(LogFileHandle, NULL, NULL, NULL, &IoStatusBlock, LogString.Buffer,LogString.Length, &buf, NULL);
		ZwClose(LogFileHandle);
	}
	else {
		DbgPrint("%s: unable to open file. error code: %x\n", DRIVER_NAME, Status);
	}
}


NTSTATUS RegistryOperationsCallback(_In_ PVOID CallbackContext, _In_opt_ PVOID Argument1, _In_opt_ PVOID Argument2)
{
	UNREFERENCED_PARAMETER(CallbackContext);
	UNREFERENCED_PARAMETER(Argument2);
	ANSI_STRING CurrentProcessName;
	ANSI_STRING CurrentOperation;
	REG_NOTIFY_CLASS RegNotifyClass = (REG_NOTIFY_CLASS)(ULONG_PTR)Argument1;
	RtlInitAnsiString(&CurrentProcessName, GetProcessImageFileName(PsGetCurrentProcess()));
	DbgPrint("%s: registry edit detected, process %s\n", DRIVER_NAME, CurrentProcessName.Buffer);
	if (RtlEqualString(&CurrentOperation, &TrackedProcessName, FALSE)) {
		RtlInitAnsiString(&CurrentOperation, GetNotifyClassString(RegNotifyClass));
		DbgPrint("%s: %s: %s\n", DRIVER_NAME, CurrentProcessName.Buffer, CurrentOperation.Buffer);
		LogToFile(&CurrentOperation);
	}
	return STATUS_SUCCESS;
}


void DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	DbgPrint("%s: driver unload routine\n", DRIVER_NAME);

	if (IsCallbackCreated) {
		NTSTATUS Status = CmUnRegisterCallback(Cookie);
		if (NT_SUCCESS(Status))	DbgPrint("%s: callback removed\n", DRIVER_NAME);
		else DbgPrint("%s: unable to remove callback\n", DRIVER_NAME);
	}
}

void LoadExternFunction() {
	UNICODE_STRING buf = RTL_CONSTANT_STRING(L"PsGetProcessImageFileName");
	GetProcessImageFileName = (GET_PROCESS_IMAGE_NAME)MmGetSystemRoutineAddress(&buf);
	if (NULL == GetProcessImageFileName)
	{
		DbgPrint("PSGetProcessImageFileName not found\n");
	}
}
void InitializeGlobals() {
	RtlInitAnsiString(&TrackedProcessName, TRACKED_PROCESS_NAME);
	RtlInitUnicodeString(&LogFileName, LOG_FILE_PATH);
}
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
	NTSTATUS Status = STATUS_SUCCESS;
	UNICODE_STRING buf;
	UNREFERENCED_PARAMETER(DriverObject);

	DbgPrint("%s: driver entry routine\n", DRIVER_NAME);
	DbgPrint("%s: %ws\n", DRIVER_NAME, RegistryPath->Buffer);

	LoadExternFunction();
	InitializeGlobals();
	RtlInitUnicodeString(&buf, L"100000");
	Status = CmRegisterCallbackEx(RegistryOperationsCallback, &buf, DriverObject, NULL, &Cookie, NULL);
	if (NT_SUCCESS(Status))
	{
		DbgPrint("%s: callback set\n", DRIVER_NAME);
		IsCallbackCreated = TRUE;
	}
	else DbgPrint("%s: unable to set callback:  %x\n", DRIVER_NAME, Status);
	DriverObject->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}