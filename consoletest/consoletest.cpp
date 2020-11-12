#include <Windows.h>
#include <stdio.h>
#define CREATION_EVENT_PATH L"Global\\ProcessCreationEvent"
#define TERMINATION_EVENT_PATH L"Global\\ProcessTerminationEvent"
#define STARTING_PROCESS L"C:\\Windows\\System32\\notepad.exe"
CRITICAL_SECTION Sync;
volatile HANDLE TrackedProcessCreationEventHandle;
volatile HANDLE TrackedProcessTerminationEventHandle;
volatile BOOLEAN IsWorking = TRUE;
volatile BOOLEAN IsProcessStarted = FALSE;
PROCESS_INFORMATION ProcessInfo;
STARTUPINFO StartupInfo;




void StartSecondaryProcess()
{
	if (!IsProcessStarted) {
		ZeroMemory(&StartupInfo, sizeof(STARTUPINFO));
		StartupInfo.cb = sizeof(STARTUPINFO);
		if (CreateProcess(STARTING_PROCESS, NULL, NULL, NULL, FALSE, CREATE_NEW_CONSOLE, NULL, NULL, &StartupInfo, &ProcessInfo) == 0) {
			printf("Error creating process %d\n", GetLastError());//не получилось - пишем код ошибки
		}
		else {
			IsProcessStarted = TRUE;
		}
	}
}
void StopSecondaryProcess()
{
	if (IsProcessStarted) {
		if (TerminateProcess(ProcessInfo.hProcess, EXIT_SUCCESS) == 0) {
			printf("Error terminating process%d\n", GetLastError());
		}
		IsProcessStarted = FALSE;
		CloseHandle(ProcessInfo.hProcess);
		CloseHandle(ProcessInfo.hThread);
		CloseHandle(StartupInfo.hStdError);
		CloseHandle(StartupInfo.hStdInput);
		CloseHandle(StartupInfo.hStdOutput);
	}
}
DWORD WINAPI CreationListener(LPVOID lpParameter)
{
	DWORD Status;
	while (IsWorking) {
		Status = WaitForSingleObject(TrackedProcessCreationEventHandle, 1000);
		if (Status == WAIT_OBJECT_0) {
			printf("Tracked process startup event received\n");
			EnterCriticalSection(&Sync);
			StartSecondaryProcess();
			LeaveCriticalSection(&Sync);
		}
	}
	return 0;
}
DWORD WINAPI TerminationListener(LPVOID lpParameter)
{
	DWORD Status;
	while (IsWorking) {
		Status = WaitForSingleObject(TrackedProcessTerminationEventHandle, 1000);
		if (Status == WAIT_OBJECT_0) {
			printf("Tracked process termination event received\n");
			EnterCriticalSection(&Sync);
			StopSecondaryProcess();
			LeaveCriticalSection(&Sync);
		}
	}
	return 0;
}
void CreateAndListen()
{
	HANDLE TrackedProcessCreationListener = CreateThread(NULL, 0, CreationListener, NULL, 0, NULL);
	HANDLE TrackedProcessTerminationListener = CreateThread(NULL, 0, TerminationListener, NULL, 0, NULL);
	if ((TrackedProcessCreationListener == NULL) || (TrackedProcessTerminationListener == NULL)) {
		printf("Unable to create listeners %d\n", GetLastError());
	}
	else {
		printf("Listeners created\n");
		getchar();
	}
	IsWorking = FALSE;
	if (TrackedProcessCreationListener != NULL) {
		WaitForSingleObject(TrackedProcessCreationListener, INFINITE);
		CloseHandle(TrackedProcessCreationListener);
	}
	if (TrackedProcessTerminationListener != NULL) {
		WaitForSingleObject(TrackedProcessTerminationListener, INFINITE);
		CloseHandle(TrackedProcessTerminationListener);
	}
}
int main()
{
	TrackedProcessCreationEventHandle = OpenEventW(SYNCHRONIZE, FALSE, CREATION_EVENT_PATH);
	TrackedProcessTerminationEventHandle = OpenEventW(SYNCHRONIZE, FALSE, TERMINATION_EVENT_PATH);
	if ((TrackedProcessCreationEventHandle == NULL) || (TrackedProcessTerminationEventHandle == NULL)) {
		printf("Unable to open events%d\n", GetLastError());
	}
	else {
		InitializeCriticalSectionAndSpinCount(&Sync, 4000);
		CreateAndListen();
	}
	if (TrackedProcessCreationEventHandle != NULL) CloseHandle(TrackedProcessCreationEventHandle);
	if (TrackedProcessTerminationEventHandle != NULL) CloseHandle(TrackedProcessTerminationEventHandle);
	getchar();
	return 0;
}