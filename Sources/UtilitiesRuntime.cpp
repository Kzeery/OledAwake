#include "UtilitiesRuntime.h"
#include <shlobj_core.h>
#include <codecvt>
#include <locale>
namespace beast = boost::beast;         
namespace http = beast::http;           
namespace websocket = beast::websocket; 
namespace net = boost::asio;            

using tcp = boost::asio::ip::tcp;     
string UtilitiesRuntime::Last_error_ = "";
string UtilitiesRuntime::Path_to_KeyFile_ = "";
string UtilitiesRuntime::MacBuffer_ = "\xff\xff\xff\xff\xff\xff";
string UtilitiesRuntime::LocalIP_ = "";
HANDLE UtilitiesRuntime::monitorOffEvent = NULL;
HANDLE UtilitiesRuntime::monitorOnEvent = NULL;
MonitorState OtherState_ = MonitorState::MONITOR_OFF;
wstring UtilitiesRuntime::widen(string& str) const
{
    return wstring_convert<codecvt_utf8<wchar_t>>().from_bytes(str);
}
UtilitiesRuntime::UtilitiesRuntime(bool& result) 
{
    result = init();
}
UtilitiesRuntime::~UtilitiesRuntime()
{

    Client_->close();
    delete Client_;
}
string UtilitiesRuntime::narrow(wstring sInput) const 
{

    // Calculate target buffer size
    long len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(),
        NULL, 0, NULL, NULL);
    if (len == 0)
        return "";

    // Convert character sequence
    string out(len, 0);
    if (len != WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), &out[0], (int)out.size(), NULL, NULL))
        return "";

    return out;
}
bool UtilitiesRuntime::init()
{
    initWOL();
    if (!createWakeupEvent())
    {
        Last_error_ = "Failed to create wakeup events! Something went wrong!";
        return false;
    }
    HMODULE hDLL = LoadLibrary(L"Shlwapi.dll");
    if (hDLL == NULL)
    {
        Last_error_ = "Failed to load Shlwapi.dll";
        return false;
    }
    PATH_APPEND_TYPE pathAppend = (PATH_APPEND_TYPE)GetProcAddress(hDLL, "PathAppendW");
    if (pathAppend == NULL)
    {
        Last_error_ = "Failed to load PathAppendW function";
        FreeLibrary(hDLL);
        return false;
    }
    TCHAR szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath)))
    {
        pathAppend(szPath, L"OledAwake");
        CreateDirectory(szPath, NULL);
        pathAppend(szPath, L"KeyFile.txt");
        Path_to_KeyFile_ = narrow(szPath);
    }
    FreeLibrary(hDLL);
    if (Path_to_KeyFile_.size() == 0)
    {
        Last_error_ = "Failed to get KeyFile path";
        return false;
    }
    if (!loadKeyFromFile())
    {
        if (!setupSessionKey())
        {
            Last_error_ = "Failed to setup session key";
            return false;
        }
        if (Key_.size() > 0)
            Handshake_ = narrow(HANDSHAKE_PAIRED);
        saveKeyToFile();
    }
    initHandshake();
    return true;
}
wstring UtilitiesRuntime::getLastError() const
{
    wstring ret = widen(Last_error_);
    Last_error_ = "";
    return ret;
}
bool UtilitiesRuntime::loadKeyFromFile()
{
    ifstream keyFile(Path_to_KeyFile_);
    if (keyFile.is_open())
    {
        if (getline(keyFile, Key_))
        {
            Handshake_ = narrow(HANDSHAKE_PAIRED);
            keyFile.close();
            return true;
        }
        keyFile.close();
    }
    Key_ = "";
    Handshake_ = narrow(HANDSHAKE_NOTPAIRED);
    return false;
}

void UtilitiesRuntime::saveKeyToFile() const
{
    ofstream keyFile(Path_to_KeyFile_);
    if (keyFile.is_open())
    {
        keyFile << Key_;
        keyFile.close();
    }
}

bool UtilitiesRuntime::setupSessionKey()
{
    try
    {
        string host = Host_;
        net::io_context ioc;
        tcp::resolver resolver{ ioc };
        websocket::stream<tcp::socket> ws{ ioc };
        beast::flat_buffer buffer;
        auto const results = resolver.resolve(host, SERVICE_PORT);
        auto ep = net::connect(ws.next_layer(), results);
        host += ':' + std::to_string(ep.port());
        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req)
            {
                req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-client-LGTVsvc");
            }));

        ws.handshake(host, "/");
        ws.write(net::buffer(std::string(Handshake_)));
        ws.read(buffer);
        ws.read(buffer);
        string t = beast::buffers_to_string(buffer.data());
        size_t u = t.find("client-key\":\"");
        if (u != string::npos)
        {
            size_t w = t.find("\"", u + 14);

            if (w != string::npos)
                Key_ = t.substr(u + 13, w - u - 13);
        }
        ws.close(websocket::close_code::normal);
        return true;
    }
    catch (const exception& e)
    {
        Last_error_ = SETUP_SESSION_KEY_ERROR;
        Last_error_.append(e.what());
        return false;
    }
}

bool UtilitiesRuntime::turnOffDisplay() const
{
    string host = Host_;
    try
    {
        time_t origtim = time(0);
        net::io_context ioc;

        
        tcp::resolver resolver{ ioc };
        websocket::stream<tcp::socket> ws{ ioc };
        auto const results = resolver.resolve(host, SERVICE_PORT);
        auto ep = net::connect(ws.next_layer(), results);
        host += ':' + std::to_string(ep.port());
        ESCAPE_TOO_MUCH_TIME(origtim, 10);

        ws.set_option(websocket::stream_base::decorator(
            [](websocket::request_type& req)
            {
                req.set(http::field::user_agent,
                    std::string(BOOST_BEAST_VERSION_STRING) +
                    " websocket-client-LGTVsvc");
            }));
        ESCAPE_TOO_MUCH_TIME(origtim, 10);
        ws.handshake(host, "/");
        ESCAPE_TOO_MUCH_TIME(origtim, 10);
        beast::flat_buffer buffer;

        ws.write(net::buffer(std::string(Handshake_)));
        ws.read(buffer); // read the response
        ws.write(net::buffer(std::string(Poweroffmess_)));
        ws.read(buffer); // read the response
        ws.close(websocket::close_code::normal);
        return true;
    }
    catch (const exception& e)
    {
        Last_error_ = TURN_OFF_DISPLAY_ERROR;
        Last_error_.append(e.what());
        return false;
    }
}

bool UtilitiesRuntime::createWakeupEvent() const
{
    monitorOnEvent = CreateEvent(NULL, TRUE, FALSE, TEXT("MonitorOn"));
    monitorOffEvent = CreateEvent(NULL, TRUE, FALSE, TEXT("MonitorOff"));

    return monitorOnEvent != NULL && monitorOffEvent != NULL;
}
void UtilitiesRuntime::initWOL()
{
    for (int i = 0; i < 16; ++i)
        MacBuffer_.append(OLED_MAC_ADDRESS);
}
void UtilitiesRuntime::initHandshake()
{
    size_t ckf = Handshake_.find(Ck_);
    Handshake_.replace(ckf, Ck_.length(), Key_);
}

bool UtilitiesRuntime::turnOnDisplay() const
{
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        return false;
    }
    const char* SendBuf = MacBuffer_.c_str();
    int BufLen = 102;

    sockaddr_in RecvAddr;
    SOCKET SendSocket = INVALID_SOCKET;
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        WSACleanup();
        return false;
    }

    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(OLED_TV_PORT);
    iResult = inet_pton(AF_INET, OLED_TV_IP, (PVOID)&RecvAddr.sin_addr);
    if (iResult != 1)
    {
        WSACleanup();
        return false;
    }

    iResult = sendto(SendSocket,
        SendBuf, BufLen, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    if (iResult == SOCKET_ERROR) {
        closesocket(SendSocket);
        WSACleanup();
        return false;
    }

    iResult = closesocket(SendSocket);
    if (iResult == SOCKET_ERROR) {
        WSACleanup();
        return false;
    }

    WSACleanup();
    return true;
}

unsigned __stdcall UtilitiesRuntime::initChatServer(void* pUserData)
{
    // Check if env variable is defined
    try
    {
        boost::asio::io_service io_service;
        tcp::endpoint endpoint(tcp::v4(), std::atoi(SERVICE_PORT));
        endpoint.address(boost::asio::ip::make_address(LocalIP_));
        ChatServer server(io_service, endpoint);
        HANDLE serverRunningEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"ServerRunning");
        if (serverRunningEvent != NULL) SetEvent(serverRunningEvent);
        else
        {
            Last_error_ = "Something went wrong trying to signal the server running event!";
            return 3;
        }
        CloseHandle(serverRunningEvent);
        io_service.run();
        return 0;
    }
    catch (std::exception& e)
    {
        Last_error_ = "Encountered an error while starting the server! Error: ";
        Last_error_.append(e.what());
        return 1;
    }

}
bool UtilitiesRuntime::ensureServerEnvironment()
{
    if (!LocalIP_.empty()) return true;
    char IPAddress[30];
    DWORD ret = GetEnvironmentVariableA("RunServer", IPAddress, 30);
    if (ret == 0)
        return false;

    LocalIP_ = IPAddress;
    return true;
}
void runClient(boost::asio::io_service* Io_service_)
{
    try
    {
        Io_service_->run();
    }
    catch (const exception& e)
    {
        cout << e.what();
        return;
    }
}
bool UtilitiesRuntime::connectClient()
{
    try
    {       
        HANDLE clientRunningEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"clientRunning");
        boost::asio::io_service io_service;
        tcp::resolver resolver(io_service);
        auto endpoint_iterator = resolver.resolve({ SERVER_IP_ADDRESS, SERVICE_PORT });
        Client_ = new ChatClient(io_service, endpoint_iterator, boost::asio::ip::host_name().c_str());
        if (clientRunningEvent != NULL)
        {
            SetEvent(clientRunningEvent);
            CloseHandle(clientRunningEvent);
        }
        else
        {
            Last_error_ = "Something went wrong trying to signal the client running event!";
            return false;
        }
        io_service.run();
    }
    catch (std::exception& e)
    {
        Last_error_ = "Error Connecting client: ";
        Last_error_.append(e.what());
        return false;
    }

    return true;
}


void UtilitiesRuntime::sendMessageToClients(MonitorState state)
{
    const char* message = state == MonitorState::MONITOR_ON ? "ON" : "OFF";
    ChatMessage msg;
    msg.body_length(strlen(message));
    std::memcpy(msg.body(), message, msg.body_length());
    msg.encode_header();
    Client_->write(msg);
}

MonitorState UtilitiesRuntime::getOtherMonitorState() const
{
    const char* stateString = Client_->getOtherState();
    return strcmp(stateString, "ON") == 0 ? MonitorState::MONITOR_ON : MonitorState::MONITOR_OFF;

}