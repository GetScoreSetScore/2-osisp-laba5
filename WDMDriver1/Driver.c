#include <ntddk.h>
#include <wdf.h>
#define DRIVER_NAME "Driver1"
DRIVER_INITIALIZE DriverEntry;
typedef PCHAR(*GET_PROCESS_IMAGE_NAME) (PEPROCESS Process);
GET_PROCESS_IMAGE_NAME gGetProcessImageFileName;
extern NTSTATUS PsLookupProcessByProcessId(HANDLE ProcessId, PEPROCESS* Process);
void CreateProcessNotifyRoutine(_In_ HANDLE ParentId, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	UNREFERENCED_PARAMETER(ParentId);
	UNREFERENCED_PARAMETER(CreateInfo);
	PEPROCESS Process;
	ANSI_STRING asProcName;
	PsLookupProcessByProcessId(ProcessId, &Process);
	RtlInitAnsiString(&asProcName, gGetProcessImageFileName(Process));
#pragma region Debug
	if (CreateInfo != NULL)
		DbgPrint("%s: %u %s started\n", DRIVER_NAME, ProcessId, asProcName.Buffer);
	else
		DbgPrint("%s: %u %s finished\n", DRIVER_NAME, ProcessId, asProcName.Buffer);
#pragma endregion

}
void LoadExternFunction() {
	UNICODE_STRING sPsGetProcessImageFileName = RTL_CONSTANT_STRING(L"PsGetProcessImageFileName");
	gGetProcessImageFileName = (GET_PROCESS_IMAGE_NAME)MmGetSystemRoutineAddress(&sPsGetProcessImageFileName);
	if (NULL == gGetProcessImageFileName)
	{
		DbgPrint("PSGetProcessImageFileName not found\n");
		return STATUS_UNSUCCESSFUL;
	}
}
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject,_In_ PUNICODE_STRING RegistryPath)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(DriverObject);
#pragma region Debug
	DbgPrint("%s: driver entry routine\n", DRIVER_NAME);
	DbgPrint("%s: %ws\n", DRIVER_NAME, RegistryPath->Buffer);
#pragma endregion
		
	LoadExternFunction();

	status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutine, FALSE);
#pragma region Debug
	if (NT_SUCCESS(status))
		DbgPrint("%s: callback set\n", DRIVER_NAME);
	else
		DbgPrint("%s: unable to set callback:  %x\n", DRIVER_NAME, status);
#pragma endregion

	return STATUS_SUCCESS;
}