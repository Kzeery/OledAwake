#include "ClientServerRuntime.h"
#include "RuntimeManager.h"
#include <iphlpapi.h>
#include "PipeCommunication.h"
typedef ULONG(WINAPI* pGetAdaptersInfo)(PIP_ADAPTER_INFO, PULONG);
std::unique_ptr<Runtime> ClientServerRuntime::Instance_ = nullptr;
bool ClientServerRuntime::InitializedOnce_ = false;

DWORD WINAPI sendMessageToServer(_In_ LPVOID);

std::unique_ptr<Runtime>& ClientServerRuntime::destroy()
{
    sendMessageToServerTimeout(RemoteRequest::SHUTDOWN_SERVER, nullptr, true, 200);
    if (ServerThread_ != nullptr && ServerThread_->joinable())
    {
        ServerThread_->join();
    }
    return Instance_;
}

void ClientServerRuntime::sendMessageToServerTimeout(RemoteRequest req, MonitorState* res, bool self, int maxTimeout) const
{
    SendServerInfoStruct serverinfo;
    serverinfo.ip = self ? LocalIP_ : OtherIP_;
    serverinfo.req = req;
    serverinfo.res = res;

    HandleWrapper hThread(CreateThread(NULL, 0, sendMessageToServer, &serverinfo, 0, NULL));
    if (hThread)
        WaitForSingleObject(hThread, maxTimeout);
}

Runtime* ClientServerRuntime::getInstance(bool newInstance)
{
    if (Instance_.get() == nullptr && !InitializedOnce_ && newInstance)
    {
        Instance_ = std::unique_ptr<Runtime>(new ClientServerRuntime);
    }
    InitializedOnce_ = true;
    return Instance_.get();
}


bool ClientServerRuntime::init()
{
    if (!ensureServerEnvironment()) return false;

    
    HandleWrapper serverRunningEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (serverRunningEvent == NULL)
    {
        Utilities::setLastError("Could not create Server Running event");
        return false;
    }
    ServerThread_ = std::unique_ptr<std::thread>(new std::thread(&ClientServerRuntime::initServer, this, serverRunningEvent));
    if (WaitForSingleObject(serverRunningEvent, 5000) == WAIT_TIMEOUT)
    {
        ServerThread_->join();
        ServerThread_.reset(nullptr);
        Utilities::setLastError("Server running event timed out!");
        return false;
    }

    return true;
}
MonitorState ClientServerRuntime::getOtherMonitorState() const
{
    MonitorState res = MonitorState::UNKNOWN;
    sendMessageToServerTimeout(RemoteRequest::GET_MONITOR_STATE, &res, false, 200);
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
    if (!LocalIP_.empty()) return true;
    LibraryWrapper hDLL(LoadLibrary(L"Iphlpapi.dll"));
    if (!hDLL)
    {
        Utilities::setLastError("EnsureServerEnvironment - Failed to load Iphlpapi.dll library");
        return false;
    }

    pGetAdaptersInfo getAdaptersInfo = (pGetAdaptersInfo)GetProcAddress(hDLL, "GetAdaptersInfo");
    if (!getAdaptersInfo)
    {
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
    if (LocalIP_.empty())
    {
        Utilities::setLastError("Something went wrong while trying to get local ip address");
        return false;
    }
    if (LocalIP_ == IPAddressA)
    {
        OtherIP_ = IPAddressB;
        IsDeviceManager_ = true;
    }
    else
    {
        OtherIP_ = IPAddressA;
        IsDeviceManager_ = false;
    }
    return true;
}

bool ClientServerRuntime::initServer(HandleWrapper& hEvent) const
{

    int iResult;
    struct addrinfo* result = NULL, * ptr = NULL, hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the local address and port to be used by the server
    iResult = getaddrinfo(LocalIP_.c_str(), ServicePort, &hints, &result);
    if (iResult != 0) {
        return false;
    }

    SOCKET ListenSocket = INVALID_SOCKET;

    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        freeaddrinfo(result);
        return false;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(ListenSocket);
        return false;
    }

    if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR) {
        freeaddrinfo(result);
        closesocket(ListenSocket);
        return false;
    }

    freeaddrinfo(result);
    SOCKET ClientSocket;
    ClientSocket = INVALID_SOCKET;

    SetEvent(hEvent);
    bool needToExit = false;
    while (true)
    {
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            break;
        }

        RemoteRequest clientRequest = RemoteRequest::REQUEST_UNKNOWN;
        MonitorState serverResponse = MonitorState::UNKNOWN;
        int iSendResult;
        iResult = recv(ClientSocket, (char*)&clientRequest, RemoteInBufferSize, 0);
        if (iResult > 0) {

            switch (clientRequest)
            {
            case RemoteRequest::GET_MONITOR_STATE:
                serverResponse = State_;
                break;
            case RemoteRequest::SHUTDOWN_SERVER:
                closesocket(ClientSocket);
                needToExit = true;
                break;
            default:
                break;
            }

            if (needToExit)
                break;

            iSendResult = send(ClientSocket, (char*)&serverResponse, RemoteOutBufferSize, 0);
            closesocket(ClientSocket);
            if (iSendResult == SOCKET_ERROR) {
                break;
            }
        }
        else
            closesocket(ClientSocket);
    }
    
    closesocket(ListenSocket);
    if(!needToExit) // Something catastrophic happened and this is not occuring as a part of the program being requested to stop.
        SetEvent(Utilities::events[SERVER_EXITED_EVENT_INDEX]);
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



DWORD WINAPI sendMessageToServer(_In_ LPVOID sendInfo)
{
    SendServerInfoStruct* info = (SendServerInfoStruct*)sendInfo;
    int iResult;


    struct addrinfo* result = NULL,
        * ptr = NULL,
        hints;

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;


    iResult = getaddrinfo(info->ip.c_str(), ServicePort, &hints, &result);
    if (iResult != 0) {
        return 1;
    }

    SOCKET ConnectSocket = INVALID_SOCKET;

    ptr = result;

    // Create a SOCKET for connecting to server
    ConnectSocket = socket(ptr->ai_family, ptr->ai_socktype,
        ptr->ai_protocol);

    if (ConnectSocket == INVALID_SOCKET) {
        freeaddrinfo(result);
        return 1;
    }

    iResult = connect(ConnectSocket, ptr->ai_addr, (int)ptr->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        ConnectSocket = INVALID_SOCKET;
    }

    freeaddrinfo(result);

    if (ConnectSocket == INVALID_SOCKET) {
        return 1;
    }

    // Send an initial buffer
    iResult = send(ConnectSocket, (char*)&info->req, RemoteInBufferSize, 0);
    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        return 1;
    }

    // shutdown the connection for sending since no more data will be sent
    // the client can still use the ConnectSocket for receiving data
    iResult = shutdown(ConnectSocket, SD_SEND);

    if (iResult == SOCKET_ERROR) {
        closesocket(ConnectSocket);
        return 1;
    }
    if (info->req != RemoteRequest::SHUTDOWN_SERVER)
        iResult = recv(ConnectSocket, (char*)info->res, RemoteOutBufferSize, 0);

    closesocket(ConnectSocket);
    return 0;
}