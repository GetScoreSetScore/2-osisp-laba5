#include <ntddk.h>
#include <wdf.h>
#define DRIVER_NAME "Driver1"
#define TRACKED_PROCESS_NAME "Calculator.exe"
#define CREATION_EVENT_PATH L"\\BaseNamedObjects\\ProcessCreationEvent"
#define TERMINATION_EVENT_PATH L"\\BaseNamedObjects\\ProcessTerminationEvent"
typedef PCHAR(*GET_PROCESS_IMAGE_NAME) (PEPROCESS Process);
GET_PROCESS_IMAGE_NAME GetProcessImageFileName;

DRIVER_INITIALIZE DriverEntry;
PDEVICE_OBJECT GlobalDeviceObject;
BOOLEAN IsCallbackCreated = FALSE;
BOOLEAN IsTrackedProcessStarted = FALSE;
BOOLEAN IsTrackedProcessCreationEventCreated = FALSE;
BOOLEAN IsTrackedProcessTerminationEventCreated = FALSE;
HANDLE TrackedProcessHandle;
ANSI_STRING TrackedProcessName;
PKEVENT TrackedProcessCreationEvent;
PKEVENT TrackedProcessTerminationEvent;
HANDLE TrackedProcessCreationEventHandle;
HANDLE TrackedProcessTerminationEventHandle;









extern NTSTATUS PsLookupProcessByProcessId(HANDLE ProcessId, PEPROCESS* Process);
void SendEvent(PKEVENT _event) {
	KeSetEvent(_event, 0, FALSE);
	KeClearEvent(_event);
}
void CreateProcessNotifyRoutine(_In_ HANDLE ParentId, _In_ HANDLE ProcessId, _Inout_opt_ PPS_CREATE_NOTIFY_INFO CreateInfo)
{
	UNREFERENCED_PARAMETER(ParentId);
	PEPROCESS Process;
	ANSI_STRING CurrentProcessName;
	BOOLEAN IsStarted = FALSE;

	PsLookupProcessByProcessId(ProcessId, &Process);
	RtlInitAnsiString(&CurrentProcessName, GetProcessImageFileName(Process));

	if (CreateInfo != NULL) {
		IsStarted = TRUE;
		DbgPrint("%s: %u %s started\n", DRIVER_NAME, ProcessId, CurrentProcessName.Buffer);
	}
	else DbgPrint("%s: %u %s finished\n", DRIVER_NAME, ProcessId, CurrentProcessName.Buffer);

	if (RtlEqualString(&CurrentProcessName, &TrackedProcessName, FALSE)) {
		if (IsStarted) {
			if (!IsTrackedProcessStarted) {
				IsTrackedProcessStarted = TRUE;
				TrackedProcessHandle = ProcessId;
				if (IsTrackedProcessCreationEventCreated) {
					DbgPrint("%s: Tracked process startup detected, sending event\n", DRIVER_NAME);
					SendEvent(TrackedProcessCreationEvent);
				}
			}
		}
		else {
			if (IsTrackedProcessStarted && (TrackedProcessHandle == ProcessId)) {
				IsTrackedProcessStarted = FALSE;
				if (IsTrackedProcessTerminationEventCreated) {
					DbgPrint("%s: Tracked process termination detected, sending event\n", DRIVER_NAME);
					SendEvent(TrackedProcessTerminationEvent);
				}
			}
		}
	}
}
void DriverUnload(IN PDRIVER_OBJECT DriverObject)
{
	UNREFERENCED_PARAMETER(DriverObject);
	DbgPrint("%s: driver unload routine\n", DRIVER_NAME);

	if (IsCallbackCreated) {
		NTSTATUS Status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutine, TRUE);
		if (NT_SUCCESS(Status))	DbgPrint("%s: callback removed\n", DRIVER_NAME);
		else DbgPrint("%s: unable to remove callback\n", DRIVER_NAME);
	}
	if (IsTrackedProcessCreationEventCreated) ZwClose(TrackedProcessCreationEventHandle);
	if (IsTrackedProcessTerminationEventCreated) ZwClose(TrackedProcessTerminationEventHandle);
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
	UNICODE_STRING buf;
	RtlInitAnsiString(&TrackedProcessName, TRACKED_PROCESS_NAME);

	RtlInitUnicodeString(&buf, CREATION_EVENT_PATH);
	TrackedProcessCreationEvent = IoCreateNotificationEvent(&buf, &TrackedProcessCreationEventHandle);
	if (NULL==TrackedProcessCreationEvent) {
		DbgPrint("%s: unable to create ProcessCreationEvent", DRIVER_NAME);
	}
	else {
		KeClearEvent(TrackedProcessCreationEvent);
		IsTrackedProcessCreationEventCreated = TRUE;
		DbgPrint("%s: created ProcessCreationEvent", DRIVER_NAME);
	}
	RtlInitUnicodeString(&buf, TERMINATION_EVENT_PATH);
	TrackedProcessTerminationEvent = IoCreateNotificationEvent(&buf, &TrackedProcessTerminationEventHandle);
	if (NULL == TrackedProcessTerminationEvent) {
		DbgPrint("%s: unable to create ProcessTerminationEvent", DRIVER_NAME);
	}
	else {
		KeClearEvent(TrackedProcessTerminationEvent);
		IsTrackedProcessTerminationEventCreated = TRUE;
		DbgPrint("%s: created ProcessTerminationEvent", DRIVER_NAME);
	}
}
NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject,_In_ PUNICODE_STRING RegistryPath)
{
	NTSTATUS Status = STATUS_SUCCESS;
	UNREFERENCED_PARAMETER(DriverObject);

	DbgPrint("%s: driver entry routine\n", DRIVER_NAME);
	DbgPrint("%s: %ws\n", DRIVER_NAME, RegistryPath->Buffer);

	LoadExternFunction();
	InitializeGlobals();
	Status = PsSetCreateProcessNotifyRoutineEx(CreateProcessNotifyRoutine, FALSE);
	if (NT_SUCCESS(Status))
	{
		DbgPrint("%s: callback set\n", DRIVER_NAME);
		IsCallbackCreated = TRUE;
	}
	else DbgPrint("%s: unable to set callback:  %x\n", DRIVER_NAME, Status);
	DriverObject->DriverUnload = DriverUnload;
	return STATUS_SUCCESS;
}