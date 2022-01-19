#include <SDKDDKVer.h>
#include <tchar.h>
#include <strsafe.h>
#include "ErrorMessages.h"
#include "RuntimeManager.h"
#include <vector>

#pragma comment(lib, "advapi32.lib")

#define SVCNAME TEXT("OledAwake")

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;

VOID SvcInstall(void);
DWORD WINAPI SvcCtrlHandler(DWORD, DWORD, LPVOID, LPVOID);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
VOID SvcReportInfo(wstring&);
VOID SvcReportEvent(LPTSTR);
VOID SvcReportEvent(wstring&);

//
// Purpose: 
//   Entry point for the process
//
// Parameters:
//   None
// 
// Return value:
//   None, defaults to 0 (zero)
//
int __cdecl _tmain(int argc, TCHAR* argv[])
{
    // If command-line parameter is "install", install the service. 
    // Otherwise, the service is probably being started by the SCM.

    if (lstrcmpi(argv[1], TEXT("install")) == 0)
    {
        SvcInstall();
        return 0;
    }
    // TO_DO: Add any additional services for the process to this table.
    SERVICE_TABLE_ENTRY DispatchTable[] =
    {
        { SVCNAME, (LPSERVICE_MAIN_FUNCTION)SvcMain },
        { NULL, NULL }
    };

    // This call returns when the service has stopped. 
    // The process should simply terminate when the call returns.

    if (!StartServiceCtrlDispatcher(DispatchTable))
    {
        SvcReportEvent(TEXT("StartServiceCtrlDispatcher"));
    }
}

//
// Purpose: 
//   Installs a service in the SCM database
//
// Parameters:
//   None
// 
// Return value:
//   None
//
VOID SvcInstall()
{
    SC_HANDLE schSCManager;
    SC_HANDLE schService;
    TCHAR szPath[MAX_PATH];

    if (!GetModuleFileName(NULL, szPath, MAX_PATH))
    {
        printf("Cannot install service (%d)\n", GetLastError());
        return;
    }

    // Get a handle to the SCM database. 

    schSCManager = OpenSCManager(
        NULL,                    // local computer
        NULL,                    // ServicesActive database 
        SC_MANAGER_ALL_ACCESS);  // full access rights 

    if (NULL == schSCManager)
    {
        printf("OpenSCManager failed (%d)\n", GetLastError());
        return;
    }
    
    // Create the service

    schService = CreateService(
        schSCManager,              // SCM database 
        SVCNAME,                   // name of service 
        SVCNAME,                   // service name to display 
        SERVICE_ALL_ACCESS,        // desired access 
        SERVICE_WIN32_OWN_PROCESS, // service type 
        SERVICE_AUTO_START,      // start type 
        SERVICE_ERROR_NORMAL,      // error control type 
        szPath,                    // path to service's binary 
        NULL,                      // no load ordering group 
        NULL,                      // no tag identifier 
        NULL,                      // no dependencies 
        NULL,                      // LocalSystem account 
        NULL);                     // no password 

    if (schService == NULL)
    {
        printf("CreateService failed (%d)\n", GetLastError());
        CloseServiceHandle(schSCManager);
        return;
    }
    else printf("Service installed successfully\n");

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
}

//
// Purpose: 
//   Entry point for the service
//
// Parameters:
//   dwArgc - Number of arguments in the lpszArgv array
//   lpszArgv - Array of strings. The first string is the name of
//     the service and subsequent strings are passed by the process
//     that called the StartService function to start the service.
// 
// Return value:
//   None.
//
VOID WINAPI SvcMain(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // Register the handler function for the service
    gSvcStatusHandle = RegisterServiceCtrlHandlerEx(
        SVCNAME,
        SvcCtrlHandler,
        NULL);

    if (!gSvcStatusHandle)
    {
        SvcReportEvent(TEXT("RegisterServiceCtrlHandler"));
        return;
    }
    // These SERVICE_STATUS members remain as set here

    gSvcStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    gSvcStatus.dwServiceSpecificExitCode = 0;

    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    SvcInit(dwArgc, lpszArgv);
}


 unsigned __stdcall waitForMonitorOnEvent(void* p_userData)
{
    UtilitiesRuntime* utilities = RuntimeManager::getUtilitiesRuntime();
    ClientServerRuntime* clientServerRuntime = RuntimeManager::getClientServerRuntime();
    HANDLE onEvent = CreateEvent(NULL, TRUE, FALSE, L"MonitorOn");
    HANDLE offEvent = CreateEvent(NULL, TRUE, FALSE, L"MonitorOff");
    HANDLE threadKillEvent = CreateEvent(NULL, TRUE, FALSE, L"KillMonitorThread");
    if (onEvent == NULL || offEvent == NULL || threadKillEvent == NULL) return 1;
    ResetEvent(onEvent);
    ResetEvent(offEvent);
    vector<HANDLE> events = { onEvent, offEvent, threadKillEvent };
    while (true)
    {
        DWORD waitRes = WaitForMultipleObjects(events.size(), events.data(), FALSE, INFINITE);
        DWORD index = waitRes - WAIT_OBJECT_0;
        switch (index)
        {
        case 0:
            clientServerRuntime->setCurrentMonitorState(MonitorState::MONITOR_ON);
            for (int i = 0; i < 10; ++i)
            {
                if (!utilities->turnOnDisplay()) SvcReportInfo(UtilitiesRuntime::getLastError());
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            ResetEvent(events[index]);
            break;
        case 1:
            clientServerRuntime->setCurrentMonitorState(MonitorState::MONITOR_OFF);
            if (clientServerRuntime->getOtherMonitorState() == MonitorState::MONITOR_OFF)
                if (!utilities->turnOffDisplay()) SvcReportEvent(UtilitiesRuntime::getLastError());
            ResetEvent(events[index]);
            break;
        default:
            CloseHandle(onEvent);
            CloseHandle(offEvent);
            CloseHandle(threadKillEvent);
            _endthreadex(0);
            return 0;
        }
    }
}

 

#define CLEAN_RESOURCES(msg, code) SvcReportEvent(msg); ReportSvcStatus(SERVICE_STOPPED, code, 0); if(powerNotifyHandle) UnregisterPowerSettingNotification(powerNotifyHandle);
#define CLEAN_AND_EXIT(msg, code) { CLEAN_RESOURCES(msg, code); return; }

VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
    // Register for power events
    HPOWERNOTIFY powerNotifyHandle = RegisterPowerSettingNotification(gSvcStatusHandle, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_SERVICE_HANDLE);
    if (powerNotifyHandle == NULL)
        CLEAN_AND_EXIT(TEXT("RegisterPowerSettingNotification"), NO_ERROR);

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 0);
    // Initialize pretty much everything in the utilities
    bool result = RuntimeManager::initAllRuntimes();
    if(!result)
        CLEAN_AND_EXIT(UtilitiesRuntime::getLastError(), NO_ERROR);

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 0);

    ghSvcStopEvent = CreateEvent(
        NULL,    // default security attributes
        TRUE,    // manual reset event
        FALSE,   // not signaled
        NULL);   // no name

    if (ghSvcStopEvent == NULL)
        CLEAN_AND_EXIT(TEXT("CreateghSvcStopEvent"), ERROR_TIMEOUT);

    uintptr_t TSE = _beginthreadex(NULL, 0, &waitForMonitorOnEvent, nullptr, 0, NULL);
    
    if (TSE == -1L)
    {
        CloseHandle(ghSvcStopEvent);
        CLEAN_AND_EXIT(TEXT("BeginMonitorThread"), ERROR_TIMEOUT);
    }

    HANDLE threadStopEvent = (HANDLE)TSE;

    // Report running status when initialization is complete.

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);
    this_thread::sleep_for(chrono::seconds(1));
    RuntimeManager::getClientServerRuntime()->setCurrentMonitorState(MonitorState::MONITOR_ON);

    // TO_DO: Perform work until service stops.
    vector<HANDLE> stopEvents = { move(threadStopEvent), move(ghSvcStopEvent) };

    while (1)
    {
        // Check whether to stop the service.
        
        DWORD res = WaitForMultipleObjects(stopEvents.size(), stopEvents.data(), FALSE, INFINITE) - WAIT_OBJECT_0;
        RuntimeManager::getClientServerRuntime()->setCurrentMonitorState(MonitorState::MONITOR_OFF);
        switch (res)
        {
        case 0: // Monitoring Thread Stopped
            ReportSvcStatus(SERVICE_STOPPED, ERROR_TIMEOUT, 0);
            break;
        default: // Received Stop Event from Windows, need to stop the monitoring thread
            ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
            HANDLE threadKillEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"KillMonitorThread");
            if (threadKillEvent != NULL)
            {
                SetEvent(threadKillEvent);
                WaitForSingleObject(stopEvents[0], 2000); // Make sure thread gets closed
                CloseHandle(threadKillEvent);
            }

        }
        UnregisterPowerSettingNotification(powerNotifyHandle);
        for (const auto& event : stopEvents)
            if (event != NULL) CloseHandle(event);

        return;
    }
}

VOID ReportSvcStatus(DWORD dwCurrentState,
    DWORD dwWin32ExitCode,
    DWORD dwWaitHint)
{
    static DWORD dwCheckPoint = 1;

    // Fill in the SERVICE_STATUS structure.

    gSvcStatus.dwCurrentState = dwCurrentState;
    gSvcStatus.dwWin32ExitCode = dwWin32ExitCode;
    gSvcStatus.dwWaitHint = dwWaitHint;

    if (dwCurrentState == SERVICE_START_PENDING)
        gSvcStatus.dwControlsAccepted = 0;
    else gSvcStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;

    if ((dwCurrentState == SERVICE_RUNNING) ||
        (dwCurrentState == SERVICE_STOPPED))
        gSvcStatus.dwCheckPoint = 0;
    else gSvcStatus.dwCheckPoint = dwCheckPoint++;

    // Report the status of the service to the SCM.
    SetServiceStatus(gSvcStatusHandle, &gSvcStatus);
}


DWORD WINAPI SvcCtrlHandler(DWORD dwCtrl, DWORD dwEventType, LPVOID lpEventData, LPVOID lpContext)
{
    // Handle the requested control code. 

    switch (dwCtrl)
    {
    case SERVICE_CONTROL_STOP:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        // Signal the service to stop.

        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);

        return NO_ERROR;
    case SERVICE_CONTROL_POWEREVENT:
    {
        POWERBROADCAST_SETTING* pbs = (POWERBROADCAST_SETTING*)lpEventData;
        if (static_cast<DWORD>(*pbs->Data) == static_cast<DWORD>(MonitorState::MONITOR_ON)) // Monitor is powered ON
        {
            HANDLE _mEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("MonitorOn"));
            if (_mEvent == NULL) return GetLastError();

            SetEvent(_mEvent);
            CloseHandle(_mEvent);
        }
        else if(static_cast<DWORD>(*pbs->Data) == static_cast<DWORD>(MonitorState::MONITOR_OFF)) // Monitor is powered OFF
        {
            HANDLE _mEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, TEXT("MonitorOff"));
            if (_mEvent == NULL) return GetLastError();

            SetEvent(_mEvent);
            CloseHandle(_mEvent);
        }
    }
        return NO_ERROR;
        
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
    
    default:
        break;
    }
    return ERROR_CALL_NOT_IMPLEMENTED;

}

VOID SvcReportInfo(wstring& error_message)
{
    size_t sz = error_message.size() + 1;
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    vector<TCHAR> Buffer(sz, 0);
    hEventSource = RegisterEventSource(NULL, SVCNAME);

    if (NULL != hEventSource)
    {
        memcpy(Buffer.data(), error_message.c_str(), sz * sizeof(wchar_t));

        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer.data();

        ReportEvent(hEventSource,        // event log handle
            EVENTLOG_INFORMATION_TYPE, // event type
            0,                   // event category
            OLED_AWAKE_INFORMATION,           // event identifier
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}


VOID SvcReportEvent(wstring& error_message)
{
    size_t sz = error_message.size() + 1;
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    vector<TCHAR> Buffer(sz, 0);
    hEventSource = RegisterEventSource(NULL, SVCNAME);

    if (NULL != hEventSource)
    {
        memcpy(Buffer.data(), error_message.c_str(), sz * sizeof(wchar_t));

        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer.data();

        ReportEvent(hEventSource,        // event log handle
            EVENTLOG_WARNING_TYPE, // event type
            0,                   // event category
            OLED_AWAKE_WARNING,           // event identifier
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data
        
        DeregisterEventSource(hEventSource);
    }
}


//
// Purpose: 
//   Logs messages to the event log
//
// Parameters:
//   szFunction - name of function that failed
// 
// Return value:
//   None
//
// Remarks:
//   The service must have an entry in the Application event log.
//
VOID SvcReportEvent(LPTSTR szFunction)
{
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    TCHAR Buffer[80];

    hEventSource = RegisterEventSource(NULL, SVCNAME);

    if (NULL != hEventSource)
    {
        StringCchPrintf(Buffer, 80, TEXT("%s failed with %d"), szFunction, GetLastError());

        lpszStrings[0] = SVCNAME;
        lpszStrings[1] = Buffer;

        ReportEvent(hEventSource,        // event log handle
            EVENTLOG_ERROR_TYPE, // event type
            0,                   // event category
            SVC_ERROR,           // event identifier
            NULL,                // no security identifier
            2,                   // size of lpszStrings array
            0,                   // no binary data
            lpszStrings,         // array of strings
            NULL);               // no binary data

        DeregisterEventSource(hEventSource);
    }
}

