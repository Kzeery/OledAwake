#include "UtilitiesRuntime.h"
#include <shlobj_core.h>

namespace beast = boost::beast;         
namespace http = beast::http;           
namespace websocket = beast::websocket; 
namespace net = boost::asio;            

using tcp = boost::asio::ip::tcp;     
string UtilitiesRuntime::Last_error_ = "";
string UtilitiesRuntime::Path_to_KeyFile_ = "";
string UtilitiesRuntime::MacBuffer_ = "\xff\xff\xff\xff\xff\xff";
HANDLE UtilitiesRuntime::monitorOffEvent = NULL;
HANDLE UtilitiesRuntime::monitorOnEvent = NULL;

UtilitiesRuntime::UtilitiesRuntime(bool& result) 
{
    result = init();
}
UtilitiesRuntime::~UtilitiesRuntime()
{
    WSACleanup();
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
    if (!initWOL())
    {
        Last_error_ = "Failed to init Wake-On-Lan related calls!";
        return false;
    }
    if (!createWakeupEvent())
    {
        Last_error_ = "Failed to create wakeup events!";
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
    return true;
}
wstring UtilitiesRuntime::getLastError() const
{
    return wstring(Last_error_.begin(), Last_error_.end());
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
    catch (const exception& const e)
    {
        Last_error_ = SETUP_SESSION_KEY_ERROR;
        Last_error_ += e.what();
        return false;
    }
}
bool UtilitiesRuntime::turnOffDisplay() const
{
    string host = Host_;
    string handshake = Handshake_;
    try
    {
        time_t origtim = time(0);
        net::io_context ioc;

        size_t ckf = handshake.find(Ck_);
        handshake.replace(ckf, Ck_.length(), Key_);
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

        ws.write(net::buffer(std::string(handshake)));
        ws.read(buffer); // read the response
        ws.write(net::buffer(std::string(Poweroffmess_)));
        ws.read(buffer); // read the response
        ws.close(websocket::close_code::normal);
        return true;
    }
    catch (exception const& e)
    {
        Last_error_ = TURN_OFF_DISPLAY_ERROR;
        Last_error_ += e.what();
        return false;
    }
}

bool UtilitiesRuntime::createWakeupEvent() const
{
    monitorOnEvent = CreateEvent(NULL, TRUE, FALSE, TEXT("MonitorOn"));
    monitorOffEvent = CreateEvent(NULL, TRUE, FALSE, TEXT("MonitorOff"));

    return monitorOnEvent != NULL && monitorOffEvent != NULL;
}
bool UtilitiesRuntime::initWOL()
{
    for (int i = 0; i < 16; ++i)
        MacBuffer_.append(OLED_MAC_ADDRESS);
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        return false;
    }
}
bool UtilitiesRuntime::turnOnDisplay() const
{
    const char* SendBuf = MacBuffer_.c_str();
    int BufLen = 102;
    int iResult;

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