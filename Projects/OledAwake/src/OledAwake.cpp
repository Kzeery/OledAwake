#include "RuntimeManager.h"
#include "ErrorMessages.h"
#include <tchar.h>
#include <strsafe.h>
#include <vector>
#include <Dbt.h>
#include <PipeCommunication.h>
#include <wchar.h>
#pragma comment(lib, "advapi32.lib")

#define SVCNAME TEXT("OledAwake")
HDEVNOTIFY deviceNotifyHandle = nullptr;
HPOWERNOTIFY powerNotifyHandle = nullptr;
SERVICE_STATUS          gSvcStatus;
SERVICE_STATUS_HANDLE   gSvcStatusHandle;
HANDLE                  ghSvcStopEvent = NULL;
GUID MouseDeviceGUID = { 0x378de44c, 0x56ef, 0x11d1,
0xbc, 0x8c, 0x00, 0xa0, 0xc9, 0x14, 0x05, 0xdd };
std::wstring MouseDeviceName = L"\\\\?\\HID#VID_046D&PID_C539&MI_01&Col01#b&edd03df&0&0000#{378de44c-56ef-11d1-bc8c-00a0c91405dd}";
bool isServer = false;
VOID SvcInstall(void);
DWORD WINAPI SvcCtrlHandler(DWORD, DWORD, LPVOID, LPVOID);
VOID WINAPI SvcMain(DWORD, LPTSTR*);

VOID ReportSvcStatus(DWORD, DWORD, DWORD);
VOID SvcInit(DWORD, LPTSTR*);
VOID SvcReportInfo(std::wstring&);
VOID SvcReportEvent(LPTSTR);
VOID SvcReportEvent(std::wstring&);
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
    gSvcStatus.dwControlsAccepted |= SERVICE_CONTROL_PRESHUTDOWN;
    // Report initial status to the SCM

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    // Perform service-specific initialization and work.

    SvcInit(dwArgc, lpszArgv);
}


 bool handleEvents(HANDLE stopEvent)
{
    TVCommunicationRuntime* communications = RuntimeManager::getTVCommunicationRuntime();
    ClientServerRuntime* clientServerRuntime = RuntimeManager::getClientServerRuntime();
    std::vector<HANDLE> events;
    for (const auto& eventName : Utilities::eventNames)
    {
        HANDLE event = CreateEvent(NULL, TRUE, FALSE, eventName);
        if (!event)
        {
            for (const auto& createdEvent : events)
                CloseHandle(event);
            return false;
        }
        events.push_back(event);
    }
    events.push_back(stopEvent);
    while (true)
    {
        DWORD waitRes = WaitForMultipleObjects(6, events.data(), FALSE, INFINITE);
        DWORD index = waitRes - WAIT_OBJECT_0;
        switch (index)
        {
        case MONITOR_ON_EVENT_INDEX: // Turn monitor on
            clientServerRuntime->setCurrentMonitorState(MonitorState::MONITOR_ON);
            for (int i = 0; i < 10; ++i)
            {
                if (!communications->turnOnDisplay()) SvcReportInfo(Utilities::getLastError());
                std::this_thread::sleep_for(std::chrono::milliseconds(25));
            }
            break;
        case MONITOR_OFF_EVENT_INDEX: // Turn monitor off
            clientServerRuntime->setCurrentMonitorState(MonitorState::MONITOR_OFF);
            if (clientServerRuntime->getOtherMonitorState() == MonitorState::MONITOR_OFF)
                if (!communications->turnOffDisplay()) SvcReportEvent(Utilities::getLastError());
            break;
        case CLIENT_SERVER_EXITED_EVENT_INDEX: // Client/server thread exited
        {
            int i = 0;
            for (; i < 20; ++i) // Try to reconnect for a maximum of 10 mins
            {
                SvcReportEvent(Utilities::getLastError());
                if (WaitForSingleObject(events[KILL_MONITOR_THREAD_INDEX], 30000) != WAIT_TIMEOUT
                    || clientServerRuntime->cleanUpAndReconnect())
                    break;
            }
            if (i < 20) break;
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
            return 0;
        }
        ResetEvent(events[index]);
    }
}

 void cleanResources()
 {
     if (powerNotifyHandle)
         UnregisterPowerSettingNotification(powerNotifyHandle);
     if (deviceNotifyHandle)
         UnregisterPowerSettingNotification(deviceNotifyHandle);
     ReportSvcStatus(SERVICE_STOPPED, NO_ERROR, 0);
 }

#define CLEAN_AND_EXIT(msg) {  SvcReportEvent(msg); cleanResources(); return; }

VOID SvcInit(DWORD dwArgc, LPTSTR* lpszArgv)
{

    powerNotifyHandle = RegisterPowerSettingNotification(gSvcStatusHandle, &GUID_CONSOLE_DISPLAY_STATE, DEVICE_NOTIFY_SERVICE_HANDLE);
    if (powerNotifyHandle == NULL)
        CLEAN_AND_EXIT(TEXT("RegisterPowerSettingNotification"));

    

    ReportSvcStatus(SERVICE_START_PENDING, NO_ERROR, 0);
    // Initialize pretty much everything in the utilities
    bool result = RuntimeManager::initAllRuntimes();
    if(!result)
        CLEAN_AND_EXIT(Utilities::getLastError());
    
    isServer = RuntimeManager::getClientServerRuntime()->isServer();
    if (isServer)
    {
        DEV_BROADCAST_DEVICEINTERFACE NotificationFilter;
        ZeroMemory(&NotificationFilter, sizeof(NotificationFilter));
        NotificationFilter.dbcc_size = sizeof(DEV_BROADCAST_DEVICEINTERFACE);
        NotificationFilter.dbcc_devicetype = DBT_DEVTYP_DEVICEINTERFACE;
        NotificationFilter.dbcc_classguid = MouseDeviceGUID;
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
    std::this_thread::sleep_for(std::chrono::seconds(1));
    RuntimeManager::getClientServerRuntime()->setCurrentMonitorState(MonitorState::MONITOR_ON);

    // TO_DO: Perform work until service stops.
    handleEvents(ghSvcStopEvent);
    RuntimeManager::getClientServerRuntime()->setCurrentMonitorState(MonitorState::MONITOR_OFF);
        
    return cleanResources();
    
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

bool DisplayTurnedOffByService = false;
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
            DWORD currentTime = GetTickCount();
            Request req = Request::GET_LAST_INPUT_TIME_DWORD;
            DWORD lastInputTime = 0;
            DWORD bytesRead = 0;
            BOOL r = CallNamedPipe(PIPE_NAME, &req, sizeof(Request), &lastInputTime, sizeof(DWORD), &bytesRead, NMPWAIT_NOWAIT);
            if (r && lastInputTime != 0)
            {
                DWORD diff = 0;
                if (lastInputTime > currentTime)
                    diff = lastInputTime - currentTime;
                else
                    diff = currentTime - lastInputTime;

                if (diff > 60000)
                {
                    std::wstring msg = L"Attempt to wakeup display while no recent user input has been detected! Posting message to turn off display.";
                    SvcReportInfo(msg);
                    req = Request::TURN_OFF_DISPLAY;
                    DWORD res = 0;
                    r = CallNamedPipe(PIPE_NAME, &req, sizeof(Request), &res, sizeof(DWORD), &bytesRead, NMPWAIT_NOWAIT);


                    if (r && res == ERROR_SUCCESS)
                        msg = L"Post message to turn off display succeeded";
                    else
                    {
                        wchar_t buffer[400];
                        LPWSTR err1, err2;

                        DWORD dw = GetLastError();
                        FormatMessageW(
                            FORMAT_MESSAGE_ALLOCATE_BUFFER |
                            FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL,
                            dw,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            (LPWSTR)&err1,
                            0, NULL);
                        FormatMessageW(
                            FORMAT_MESSAGE_ALLOCATE_BUFFER |
                            FORMAT_MESSAGE_FROM_SYSTEM |
                            FORMAT_MESSAGE_IGNORE_INSERTS,
                            NULL,
                            res,
                            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                            (LPWSTR)&err2,
                            0, NULL);
                        _snwprintf_s(buffer, 400, _TRUNCATE, L"Something went wrong trying to send message to turn off display.\n Err1: (%s)\n Err2: (%s)", err1, err2);
                        LocalFree(err1);
                        LocalFree(err2);
                        msg =  buffer;
                    }
                    
                    SvcReportInfo(msg);
                    DisplayTurnedOffByService = true;
                    break;
                }
            }
            eventName = Utilities::eventNames[MONITOR_ON_EVENT_INDEX];
            break;
        }
        break;
        case MonitorState::MONITOR_OFF:
            eventName = DisplayTurnedOffByService ? L"" : Utilities::eventNames[MONITOR_OFF_EVENT_INDEX];
            DisplayTurnedOffByService = false;
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
        if (!isServer) break;
        PDEV_BROADCAST_DEVICEINTERFACE deviceInfo = (PDEV_BROADCAST_DEVICEINTERFACE)lpEventData;
        switch (dwEventType)
        {
        case DBT_DEVICEARRIVAL:
            if (deviceInfo->dbcc_name == MouseDeviceName)
                eventName = Utilities::eventNames[SWITCH_HDMI1_EVENT_INDEX];
            break;


        case DBT_DEVICEREMOVECOMPLETE:
            if (deviceInfo->dbcc_name == MouseDeviceName)
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

        // Signal the service to stop.

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

