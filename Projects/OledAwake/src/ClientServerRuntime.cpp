#include "ClientServerRuntime.h"
#include "RuntimeManager.h"
#include <iphlpapi.h>
#include "PipeCommunication.h"
typedef ULONG(WINAPI* pGetAdaptersInfo)(PIP_ADAPTER_INFO, PULONG);
std::unique_ptr<Runtime> ClientServerRuntime::Instance_ = nullptr;

ClientServerRuntime::~ClientServerRuntime()
{
    RemoteRequest req = RemoteRequest::QUIT;
    MonitorState res = MonitorState::MONITOR_OFF;
    DWORD bytesRead = 0;
    CallNamedPipe(REMOTE_PIPE_NAME("."), &req, REMOTE_IN_BUFFER_SIZE, &res, REMOTE_OUT_BUFFER_SIZE, &bytesRead, NMPWAIT_NOWAIT);
    if (PipeServerThread_ != nullptr && PipeServerThread_->joinable())
    {
        PipeServerThread_->join();
    }
}

Runtime* ClientServerRuntime::getInstance()
{
    if (Instance_.get() == nullptr)
    {
        Instance_ = std::unique_ptr<Runtime>(new ClientServerRuntime);
    }
    return Instance_.get();
}

bool ClientServerRuntime::init()
{
    if (!ensureServerEnvironment()) return false;


    HANDLE serverRunningEvent = CreateEvent(NULL, TRUE, FALSE, L"ServerRunning");
    if (serverRunningEvent == NULL)
    {
        Utilities::setLastError("Could not create Server Running event");
        return false;
    }

    PipeServerThread_ = std::unique_ptr<std::thread>(new std::thread(&ClientServerRuntime::initServer, this));
    if (WaitForSingleObject(serverRunningEvent, 5000) == WAIT_TIMEOUT)
    {
        PipeServerThread_->join();
        PipeServerThread_.reset(nullptr);
        Utilities::setLastError("Server running event timed out!");
        CloseHandle(serverRunningEvent);
        return false;
    }
    CloseHandle(serverRunningEvent);
    
    return true;
}

MonitorState ClientServerRuntime::getOtherMonitorState() const
{
    RemoteRequest req = RemoteRequest::GET_MONITOR_STATE;
    MonitorState res = MonitorState::MONITOR_OFF;
    DWORD bytesRead = 0;
    CallNamedPipeW(RemotePipeName_.c_str(), &req, REMOTE_IN_BUFFER_SIZE, &res, REMOTE_OUT_BUFFER_SIZE, &bytesRead, NMPWAIT_NOWAIT);
    return res;
}

MonitorState ClientServerRuntime::getCurrentMonitorState()
{
    return State_;
}

void ClientServerRuntime::setCurrentMonitorState(MonitorState state)
{
    State_ = state;
}

bool ClientServerRuntime::ensureServerEnvironment()
{

    HMODULE hDLL = LoadLibrary(L"Iphlpapi.dll");
    if (!hDLL)
    {
        Utilities::setLastError("EnsureServerEnvironment - Failed to load Iphlpapi.dll library");
        return false;
    }

    pGetAdaptersInfo getAdaptersInfo = (pGetAdaptersInfo)GetProcAddress(hDLL, "GetAdaptersInfo");
    if (!getAdaptersInfo)
    {
        FreeLibrary(hDLL);
        Utilities::setLastError("EnsureServerEnvironment - failed to load GetAdaptersInfo function from DLL");
        return false;
    }

    ULONG ulOutBufLen = sizeof(IP_ADAPTER_INFO);
    std::unique_ptr<char> adapterData(new char[ulOutBufLen]);
    PIP_ADAPTER_INFO pAdapter = (PIP_ADAPTER_INFO)adapterData.get();
    ULONG ret = 0;
    if ((ret = getAdaptersInfo(pAdapter, &ulOutBufLen)) == ERROR_BUFFER_OVERFLOW)
    {
        adapterData.reset(new char[ulOutBufLen]);
        pAdapter = (PIP_ADAPTER_INFO)adapterData.get();
        if (getAdaptersInfo(pAdapter, &ulOutBufLen) == NO_ERROR)
        {
            while (pAdapter)
            {
                std::string ipString = pAdapter->IpAddressList.IpAddress.String;
                if (ipString.size() > 3 && ipString.substr(0, 3) == "192")
                {
                    LocalIP_ = std::move(ipString);
                    break;
                }
                pAdapter = pAdapter->Next;
            }
        }
    }
    else if (ret == NO_ERROR)
    {
        std::string ipString = pAdapter->IpAddressList.IpAddress.String;
        if (ipString.size() > 3 && ipString.substr(0, 3) == "192")
            LocalIP_ = std::move(ipString);
    }
    FreeLibrary(hDLL);
    if (LocalIP_.empty())
    {
        Utilities::setLastError("Something went wrong while trying to get local ip address");
        return false;
    }
    if (LocalIP_ == IP_ADDRESS_A)
    {
        RemotePipeName_ = REMOTE_PIPE_NAME(IP_ADDRESS_B);
        IsDeviceManager_ = true;
    }
    else
    {
        RemotePipeName_ = REMOTE_PIPE_NAME(IP_ADDRESS_A);
        IsDeviceManager_ = false;
    }
    return true;
}
bool ClientServerRuntime::initServer()
{
    bool firstRun = true;
    bool signalEvent = false;
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    while (true)
    {

        SECURITY_ATTRIBUTES attrib;
        std::unique_ptr<SECURITY_DESCRIPTOR> desc(new SECURITY_DESCRIPTOR{ 0 });
        InitializeSecurityDescriptor(desc.get(), SECURITY_DESCRIPTOR_REVISION);
        SetSecurityDescriptorDacl(desc.get(), TRUE, (PACL)NULL, FALSE);

        attrib.nLength = sizeof(SECURITY_ATTRIBUTES);
        attrib.bInheritHandle = TRUE;
        attrib.lpSecurityDescriptor = desc.get();

        hPipe = CreateNamedPipe(REMOTE_PIPE_NAME("."), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, REMOTE_OUT_BUFFER_SIZE, REMOTE_IN_BUFFER_SIZE, NULL, &attrib);
        if (hPipe == INVALID_HANDLE_VALUE)
        {
            signalEvent = true;
            break;
        }

        if (firstRun)
        {
            firstRun = false;
            HANDLE serverRunningEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"ServerRunning");
            if (serverRunningEvent == NULL)
            {
                signalEvent = true;
                break;
            }
            SetEvent(serverRunningEvent);
            CloseHandle(serverRunningEvent);
        }

        BOOL fConnected = ConnectNamedPipe(hPipe, NULL) ?
            TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
        if (fConnected)
        {
            if (!HandleConnection(hPipe))
                break;
        }
        else
            CloseHandle(hPipe);
    }

    if (signalEvent)
    {
        HANDLE serverExitedEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, Utilities::eventNames[SERVER_EXITED_EVENT_INDEX]);
        if (!serverExitedEvent) return false;
        SetEvent(serverExitedEvent);
        CloseHandle(serverExitedEvent);
    }
    return true;
}

void WINAPI cleanup(HANDLE& hPipe)
{
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}

bool ClientServerRuntime::HandleConnection(HANDLE& hPipe)
{
    std::unique_ptr<RemoteRequest> pchRequest(new RemoteRequest);
    std::unique_ptr<MonitorState> pchReply(new MonitorState);

    DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
    BOOL fSuccess = FALSE;

    fSuccess = ReadFile(
        hPipe,        // handle to pipe 
        pchRequest.get(),    // buffer to receive data 
        REMOTE_IN_BUFFER_SIZE, // size of buffer 
        &cbBytesRead, // number of bytes read 
        NULL);        // not overlapped I/O 

    if (!fSuccess || cbBytesRead == 0)
    {
        cleanup(hPipe);
        return true;
    }

    switch (*pchRequest)
    {
    case RemoteRequest::GET_MONITOR_STATE:
        *pchReply = RuntimeManager::getClientServerRuntime()->getCurrentMonitorState();
        break;
    default:
        cleanup(hPipe);
        return false;
    }
    // Process the incoming message.

    // Write the reply to the pipe. 
    fSuccess = WriteFile(
        hPipe,        // handle to pipe 
        pchReply.get(),     // buffer to write from 
        REMOTE_OUT_BUFFER_SIZE, // number of bytes to write 
        &cbWritten,   // number of bytes written 
        NULL);        // not overlapped I/O 

    if (!fSuccess || REMOTE_OUT_BUFFER_SIZE != cbWritten)
    {
        cleanup(hPipe);
        return true;
    }

    // Flush the pipe to allow the client to read the pipe's contents 
    // before disconnecting. Then disconnect the pipe, and close the 
    // handle to this pipe instance. 
    FlushFileBuffers(hPipe);
    cleanup(hPipe);
    return true;
}

bool ClientServerRuntime::getTimeSinceLastUserInput(ULONGLONG& tOut) const
{
    Request req = Request::GET_TIME_SINCE_LAST_INPUT;
    ULONGLONG lastInputTime = 0;
    DWORD bytesRead = 0;
    BOOL r = CallNamedPipe(PIPE_NAME, &req, sizeof(Request), &lastInputTime, sizeof(ULONGLONG), &bytesRead, NMPWAIT_NOWAIT);
    if (!r)
    {
        LPVOID buffer;
        DWORD dw = GetLastError();
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&buffer,
            0, NULL);
        Utilities::setLastError(std::string("CallNamedPipe to OledAwakeApp failed - ") + (LPSTR)buffer);
        LocalFree(buffer);
        return false;
    }
    else if (lastInputTime == 0 || bytesRead != sizeof(ULONGLONG))
    {
        Utilities::setLastError("Oled Awake app returned an invalid value for last input time!");
        return false;
    }
    tOut = lastInputTime;
    return true;
}

bool ClientServerRuntime::postWindowsTurnOffDisplayMessage() const
{
    Request req = Request::TURN_OFF_DISPLAY;
    DWORD res = 0;
    DWORD bytesRead = 0;
    DWORD r = CallNamedPipe(PIPE_NAME, &req, sizeof(Request), &res, sizeof(DWORD), &bytesRead, NMPWAIT_NOWAIT);
    std::string err;
    if (!r || bytesRead != sizeof(DWORD))
    {
        LPVOID buffer;
        DWORD dw = GetLastError();
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&buffer,
            0, NULL);
        Utilities::setLastError(std::string("CallNamedPipe to OledAwakeApp failed - ") + (LPSTR)buffer);
        LocalFree(buffer);
        return false;
    }
    else if (res != ERROR_SUCCESS)
    {
        LPVOID buffer;
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            res,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&buffer,
            0, NULL);
        Utilities::setLastError(std::string("OledAwakeApp PostMessage failed - ") + (LPSTR)buffer);
        LocalFree(buffer);
        return false;
    }
    return true;
}