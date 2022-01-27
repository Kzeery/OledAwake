#include <SDKDDKVer.h>
#define WIN32_LEAN_AND_MEAN
#include "RuntimeManager.h"
#include "ErrorMessages.h"
#include "PipeCommunication.h"
#include <tchar.h>
#include <strsafe.h>
#include <Dbt.h>
#pragma comment(lib, "advapi32.lib")
#define SVCNAME TEXT("OledAwake")

HDEVNOTIFY deviceNotifyHandle = nullptr;
HPOWERNOTIFY powerNotifyHandle = nullptr;

SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;

GUID mouseDeviceGUID = { 0x378de44c, 0x56ef, 0x11d1,
0xbc, 0x8c, 0x00, 0xa0, 0xc9, 0x14, 0x05, 0xdd };
std::wstring mouseDeviceName = L"\\\\?\\HID#VID_046D&PID_C539&MI_01&Col01#b&edd03df&0&0000#{378de44c-56ef-11d1-bc8c-00a0c91405dd}";

bool displayTurnedOffByService = false;

VOID SvcInstall(void);
DWORD WINAPI SvcCtrlHandler(DWORD, DWORD, LPVOID, LPVOID);
VOID WINAPI SvcMain(DWORD, LPTSTR*);
BOOL HandleEvents(HANDLE);
VOID CleanResources();
VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
VOID SvcReportInfo();
VOID SvcReportInfo(std::wstring&);
VOID SvcReportEvent(LPTSTR);
VOID SvcReportEvent(std::wstring&);

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
    gSvcStatus.dwControlsAccepted |= SERVICE_CONTROL_PRESHUTDOWN;
    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    SvcInit(dwArgc, lpszArgv);
}

#define CLEAN_AND_EXIT(msg) {  SvcReportEvent(msg); CleanResources(); return; }
VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{
    bool result = RuntimeManager::initAllRuntimes();
    if (!result)
        CLEAN_AND_EXIT(Utilities::getLastError());

    powerNotifyHandle = RegisterPowerSettingNotification(gSvcStatusHandle, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_SERVICE_HANDLE);
    if (powerNotifyHandle == NULL)
        CLEAN_AND_EXIT(TEXT("RegisterPowerSettingNotification"));

    
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 0);
    // Initialize pretty much everything in the utilities
    
    if (RuntimeManager::getClientServerRuntime()->isDeviceManager())
    {
        DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
        ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
        NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        NotificationFilter.dbcc_classguid = mouseDeviceGUID;
        deviceNotifyHandle = RegisterDeviceNotification(gSvcStatusHandle, &NotificationFilter, DEVICE_NOTIFY_SERVICE_HANDLE);
        if (deviceNotifyHandle == NULL)
            CLEAN_AND_EXIT(TEXT("RegisterDeviceNotifiaction"));
    }
    
    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 0);

    ghSvcStopEvent = CreateEvent(
        NULL,    // default security attributes
        TRUE,    // manual reset event
        FALSE,   // not signaled
        NULL);   // no name

    if (ghSvcStopEvent == NULL)
        CLEAN_AND_EXIT(TEXT("CreateghSvcStopEvent"));



    // Report running status when initialization is complete.

    ReportSvcStatus(SERVICE_RUNNING, NO_ERROR, 0);

    // TO_DO: Perform work until service stops.
    HandleEvents(ghSvcStopEvent);
        
    return CleanResources();
    
}

BOOL HandleEvents(HANDLE stopEvent)
{
    TVCommunicationRuntime* communications = RuntimeManager::getTVCommunicationRuntime();
    ClientServerRuntime* clientServerRuntime = RuntimeManager::getClientServerRuntime();
    std::vector<HANDLE> events;
    for (int i = 0; i < EVENT_NAMES_SIZE; ++i)
    {
        HANDLE event = CreateEvent(NULL, TRUE, FALSE, Utilities::eventNames[i]);
        if (!event)
        {
            for (const auto& createdEvent : events)
                CloseHandle(event);
            return FALSE;
        }
        events.push_back(event);

    }
    events.push_back(stopEvent);
    while (true)
    {
        DWORD index = WaitForMultipleObjects(EVENT_NAMES_SIZE + 1, events.data(), FALSE, INFINITE) - WAIT_OBJECT_0;
        switch (index)
        {
        case MONITOR_ON_EVENT_INDEX: // Turn monitor on
            clientServerRuntime->setCurrentMonitorState(MonitorState::MONITOR_ON);
            for (int i = 0; i < 10; ++i)
            {
                if (!communications->turnOnDisplay()) SvcReportInfo();
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            break;
        case MONITOR_OFF_EVENT_INDEX: // Turn monitor off
            clientServerRuntime->setCurrentMonitorState(MonitorState::MONITOR_OFF);
            if (clientServerRuntime->getOtherMonitorState() != MonitorState::MONITOR_ON)
                if (!communications->turnOffDisplay()) SvcReportEvent(Utilities::getLastError());
            break;
        case SERVER_EXITED_EVENT_INDEX: // Client/server thread exited
        {
            SetEvent(events[EXIT_SERVICE_EVENT_INDEX]);
            break;
        }
        case SWITCH_HDMI1_EVENT_INDEX: // Switch to HDMI 1
            if (!communications->switchInput(Inputs::HDMI1))
                SvcReportEvent(Utilities::getLastError());
            break;
        case SWITCH_HDMI2_EVENT_INDEX: // Switch to HDMI 2
            if (!communications->switchInput(Inputs::HDMI2))
                SvcReportEvent(Utilities::getLastError());
            break;
        default: // received stop event
            for (auto& event : events)
                CloseHandle(event);
            return FALSE;
        }
        ResetEvent(events[index]);
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
    std::wstring eventName = L"";
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

        PPOWERBROADCAST_SETTING pbs = (PPOWERBROADCAST_SETTING)lpEventData;
        switch (static_cast<MonitorState>(*pbs->Data))
        {
        case MonitorState::MONITOR_ON:
        {
            ULONGLONG timeSinceLastUserInput = 0;
            if (RuntimeManager::getClientServerRuntime()->getTimeSinceLastUserInput(timeSinceLastUserInput))
            {
                if (timeSinceLastUserInput > 90000)
                {
                    if (RuntimeManager::getClientServerRuntime()->postWindowsTurnOffDisplayMessage())
                    {
                        std::wstring msg = L"Posted message to windows to turn off display because no user input detected recently.";
                        SvcReportInfo(msg);
                        displayTurnedOffByService = true;
                        break;
                    }
                    else
                        SvcReportInfo();
                }
            }
            else
                SvcReportInfo();
            
            eventName = Utilities::eventNames[MONITOR_ON_EVENT_INDEX];
            break;
        }
        break;
        case MonitorState::MONITOR_OFF:
            eventName = displayTurnedOffByService ? L"" : Utilities::eventNames[MONITOR_OFF_EVENT_INDEX];
            displayTurnedOffByService = false;
            break;
        default:
            break;
        }
        if (eventName.empty()) return NO_ERROR;

        HANDLE event = OpenEvent(EVENT_ALL_ACCESS, FALSE, eventName.c_str());
        if (event == NULL) return GetLastError();

        SetEvent(event);
        CloseHandle(event);

        return NO_ERROR;
    }
    case SERVICE_CONTROL_DEVICEEVENT:
    {
        if (!RuntimeManager::getClientServerRuntime()->isDeviceManager()) break;
        PDEV_BROADCAST_DEVICEINTERFACE deviceInfo = (PDEV_BROADCAST_DEVICEINTERFACE)lpEventData;
        switch (dwEventType)
        {
        case DBT_DEVICEARRIVAL:
            if (deviceInfo->dbcc_name == mouseDeviceName)
                eventName = Utilities::eventNames[SWITCH_HDMI1_EVENT_INDEX];
            break;


        case DBT_DEVICEREMOVECOMPLETE:
            if (deviceInfo->dbcc_name == mouseDeviceName)
                eventName = Utilities::eventNames[SWITCH_HDMI2_EVENT_INDEX];
            break;
        default:
            break;
        }

        if (eventName.empty()) return NO_ERROR;

        HANDLE event = OpenEvent(EVENT_ALL_ACCESS, FALSE, eventName.c_str());
        if (event == NULL) return GetLastError();

        SetEvent(event);
        CloseHandle(event);
        return NO_ERROR;

    }
    case SERVICE_CONTROL_PRESHUTDOWN:
        ReportSvcStatus(SERVICE_STOP_PENDING, NO_ERROR, 0);

        SetEvent(ghSvcStopEvent);
        ReportSvcStatus(gSvcStatus.dwCurrentState, NO_ERROR, 0);
        return NO_ERROR;
        break;
    case SERVICE_CONTROL_INTERROGATE:
        return NO_ERROR;
        break;
    default:
        break;
    }
    return ERROR_CALL_NOT_IMPLEMENTED;

}
VOID SvcReportInfo()
{
    return SvcReportInfo(Utilities::getLastError());
}
VOID SvcReportInfo(std::wstring& error_message)
{
    size_t sz = error_message.size() + 1;
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    std::vector<TCHAR> Buffer(sz, 0);
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


VOID SvcReportEvent(std::wstring& error_message)
{
    size_t sz = error_message.size() + 1;
    HANDLE hEventSource;
    LPCTSTR lpszStrings[2];
    std::vector<TCHAR> Buffer(sz, 0);
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


VOID CleanResources()
{
    if (powerNotifyHandle)
        UnregisterPowerSettingNotification(powerNotifyHandle);
    if (deviceNotifyHandle)
        UnregisterPowerSettingNotification(deviceNotifyHandle);
    ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
}