#include "UtilitiesRuntime.h"
#include <shlobj_core.h>
#include <codecvt>
#include <locale>
namespace beast = boost::beast;         
namespace http = beast::http;           
namespace websocket = beast::websocket; 
namespace net = boost::asio;            

using tcp = boost::asio::ip::tcp;     
string UtilitiesRuntime::LastError_ = "";
string UtilitiesRuntime::Path_to_KeyFile_ = "";
string UtilitiesRuntime::MacBuffer_ = "\xff\xff\xff\xff\xff\xff";
unique_ptr<Runtime> UtilitiesRuntime::Instance_ = nullptr;

UtilitiesRuntime::UtilitiesRuntime()
{
}

Runtime* UtilitiesRuntime::getInstance()
{
    if (Instance_.get() == nullptr)
    {
        Instance_ = unique_ptr<Runtime>(new UtilitiesRuntime);
    }
    return Instance_.get();
}

bool UtilitiesRuntime::init()
{
    initWOL();

    HMODULE hDLL = LoadLibrary(L"Shlwapi.dll");
    if (hDLL == NULL)
        SET_ERROR_EXIT("Failed to load Shlwapi.dll", false);

    PATH_APPEND_TYPE pathAppend = (PATH_APPEND_TYPE)GetProcAddress(hDLL, "PathAppendW");
    if (pathAppend == NULL)
    {
        LastError_ = "Failed to load PathAppendW function";
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
        SET_ERROR_EXIT("Failed to get KeyFile path", false);

    if (!loadKeyFromFile())
    {
        if (!setupSessionKey())
            SET_ERROR_EXIT("Failed to setup session key", false);
        if (Key_.empty())
            SET_ERROR_EXIT("Received invalid session key", false)
        Handshake_ = narrow(HANDSHAKE_PAIRED);
        saveKeyToFile();
    }
    initHandshake();
    return true;
}

void UtilitiesRuntime::initWOL()
{
    for (int i = 0; i < 16; ++i)
        MacBuffer_.append(OLED_MAC_ADDRESS);
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

void UtilitiesRuntime::initHandshake()
{
    size_t ckf = Handshake_.find(Ck_);
    Handshake_.replace(ckf, Ck_.length(), Key_);
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
        SET_ERROR_EXIT(string("Failed to set up session key with Oled TV. Please check config. Error Message: ") + e.what(), false);
}


wstring UtilitiesRuntime::getLastError()
{
    wstring ret = widen(LastError_);
    LastError_ = "";
    return ret;
}
void UtilitiesRuntime::setLastError(string error)
{
    LastError_ = move(error);
}


wstring UtilitiesRuntime::widen(string& str)
{
    return wstring_convert<codecvt_utf8<wchar_t>>().from_bytes(str);
}


string UtilitiesRuntime::narrow(wstring sInput) 
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
    }
    catch (const exception& e)
        SET_ERROR_EXIT(string("Failed to turn off display. Encountered an error. Please check config. Error Message: ") + e.what(), false);
    return true;

}

bool UtilitiesRuntime::turnOnDisplay() const
{
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        LastError_ = "Failed WSAStartup";
        return false;
    }
    const char* SendBuf = MacBuffer_.c_str();
    int BufLen = 102;

    sockaddr_in RecvAddr;
    SOCKET SendSocket = INVALID_SOCKET;
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        LastError_ = "Failed to initialize socket";
        WSACleanup();
        return false;
    }
    BOOL val = true;
    if (setsockopt(SendSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&val, sizeof(BOOL)) != 0)
    {
        LastError_ = "Failed to set socket options";
        WSACleanup();
        return false;
    }
    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(OLED_TV_PORT);
    iResult = InetPton(AF_INET, BROADCAST_ADDRESS, &RecvAddr.sin_addr.s_addr);
    if (iResult != 1)
    {
        LastError_ = "Failed to convert IP string to address";
        WSACleanup();
        return false;
    }

    iResult = sendto(SendSocket,
        SendBuf, BufLen, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    if (iResult == SOCKET_ERROR) {
        LastError_ = "Failed to send socket";
        closesocket(SendSocket);
        WSACleanup();
        return false;
    }

    iResult = closesocket(SendSocket);
    if (iResult == SOCKET_ERROR) {
        LastError_ = "Failed to cleanup socket";
        WSACleanup();
        return false;
    }

    WSACleanup();
    return true;
}

