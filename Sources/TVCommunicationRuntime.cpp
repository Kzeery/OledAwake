#include "TVCommunicationRuntime.h"
#include <shlobj_core.h>
#include <codecvt>
#include <locale>
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

std::unique_ptr<Runtime> TVCommunicationRuntime::Instance_ = nullptr;

Runtime* TVCommunicationRuntime::getInstance()
{
    if (Instance_.get() == nullptr)
    {
        Instance_ = std::unique_ptr<Runtime>(new TVCommunicationRuntime);
    }
    return Instance_.get();
}

bool TVCommunicationRuntime::init()
{
    if (!ensureTVMacAddress()) 
        return false;

    HMODULE hDLL = LoadLibrary(L"Shlwapi.dll");
    if (hDLL == NULL)
        SET_ERROR_EXIT("Failed to load Shlwapi.dll", false);

    PATH_APPEND_TYPE pathAppend = (PATH_APPEND_TYPE)GetProcAddress(hDLL, "PathAppendW");
    if (pathAppend == NULL)
    {
        Utilities::setLastError("Failed to load PathAppendW function");
        FreeLibrary(hDLL);
        return false;
    }
    TCHAR szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPath(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath)))
    {
        pathAppend(szPath, L"OledAwake");
        CreateDirectory(szPath, NULL);
        pathAppend(szPath, L"KeyFile.txt");
        Path_to_KeyFile_ = Utilities::narrow(szPath);
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
            Handshake_ = Utilities::narrow(HANDSHAKE_PAIRED);
        saveKeyToFile();
    }
    initHandshake();
    return true;
}

bool TVCommunicationRuntime::loadKeyFromFile()
{
    std::ifstream keyFile(Path_to_KeyFile_);
    if (keyFile.is_open())
    {
        if (std::getline(keyFile, Key_))
        {
            Handshake_ = Utilities::narrow(HANDSHAKE_PAIRED);
            keyFile.close();
            return true;
        }
        keyFile.close();
    }
    Key_ = "";
    Handshake_ = Utilities::narrow(HANDSHAKE_NOTPAIRED);
    return false;
}

void TVCommunicationRuntime::saveKeyToFile() const
{
    std::ofstream keyFile(Path_to_KeyFile_);
    if (keyFile.is_open())
    {
        keyFile << Key_;
        keyFile.close();
    }
}

void TVCommunicationRuntime::initHandshake()
{
    size_t ckf = Handshake_.find(HANDSHAKE_IDENTIFIER);
    Handshake_.replace(ckf, strlen(HANDSHAKE_IDENTIFIER), Key_);
}

bool TVCommunicationRuntime::setupSessionKey()
{
    std::string host = OLED_TV_IP;

    try
    {
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
        std::string t = beast::buffers_to_string(buffer.data());
        size_t u = t.find("client-key\":\"");
        if (u != std::string::npos)
        {
            size_t w = t.find("\"", u + 14);

            if (w != std::string::npos)
                Key_ = t.substr(u + 13, w - u - 13);
        }
        ws.close(websocket::close_code::normal);
        return true;
    }
    catch (const std::exception& e)
        SET_ERROR_EXIT(std::string("Failed to set up session key with Oled TV. Please check config. Error Message: ") + e.what(), false);
}

bool TVCommunicationRuntime::turnOnDisplay() const
{
    WSADATA wsaData;
    int iResult;

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        Utilities::setLastError("Failed WSAStartup");
        return false;
    }
    const char* SendBuf = MacBuffer_.c_str();
    int BufLen = 102;

    sockaddr_in RecvAddr;
    SOCKET SendSocket = INVALID_SOCKET;
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        Utilities::setLastError("Failed to initialize socket");
        WSACleanup();
        return false;
    }
    BOOL val = true;
    if (setsockopt(SendSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&val, sizeof(BOOL)) != 0)
    {
        Utilities::setLastError("Failed to set socket options");
        WSACleanup();
        return false;
    }

    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(OLED_TV_PORT);
    iResult = InetPton(AF_INET, TEXT(BROADCAST_ADDRESS), &RecvAddr.sin_addr.s_addr);
    if (iResult != 1)
    {
        Utilities::setLastError("Failed to convert IP string to address");
        WSACleanup();
        return false;
    }

    iResult = sendto(SendSocket,
        SendBuf, BufLen, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    if (iResult == SOCKET_ERROR) {
        Utilities::setLastError("Failed to send socket");
        closesocket(SendSocket);
        WSACleanup();
        return false;
    }

    iResult = closesocket(SendSocket);
    if (iResult == SOCKET_ERROR) {
        Utilities::setLastError("Failed to send socket");
        WSACleanup();
        return false;
    }

    WSACleanup();
    return true;
}
bool TVCommunicationRuntime::sendMessageToTV(const char* input) const
{
    try
    {
        std::string host = OLED_TV_IP;
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
        ws.write(net::buffer(std::string(input)));
        ws.read(buffer); // read the response
    }
    catch (const std::exception& e)
    {
        char buffer[400];
        snprintf(buffer, 400, "Failed to send message to tv. Encountered an error.\nMessage sent: (%s)\nError Message: (%s)", e.what(), input);
        SET_ERROR_EXIT(buffer, false);
    }
    return true;
}

bool TVCommunicationRuntime::turnOffDisplay() const
{
    return sendMessageToTV(TURN_OFF_TV_MESSAGE);
}

bool TVCommunicationRuntime::ensureTVMacAddress()
{
    if (MacBuffer_.size() > 6) return true;

    char mac[6];
    in_addr destIP;
    in_addr srcIP;
    srcIP.s_addr = INADDR_ANY;

    ULONG macAddress[2];
    ULONG physicalAddressLength = 6;

    WSADATA wsaData;

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        Utilities::setLastError("Error starting WSA");
        return false;	//Return 1 on error
    }

    HINSTANCE hDLL = LoadLibrary(L"iphlpapi.dll");
    if (hDLL == NULL)
    {
        WSACleanup();
        Utilities::setLastError("Error loading iphlapi library");
        return false;
    }
    SendArpType SendArp = (SendArpType)GetProcAddress(hDLL, "SendARP");

    if (SendArp == NULL)
    {
        WSACleanup();
        FreeLibrary(hDLL);
        Utilities::setLastError("Error getting process address of GetArp function");
        return false;
    }

    int iResult = InetPton(AF_INET, TEXT(OLED_TV_IP), &destIP.s_addr);
    if (iResult != 1)
    {
        FreeLibrary(hDLL);
        Utilities::setLastError("Failed to convert IP string to address");
        WSACleanup();
        return false;
    }

    DWORD ret = SendArp(destIP, srcIP, macAddress, &physicalAddressLength);

    FreeLibrary(hDLL);
    WSACleanup();

    if (ret != NO_ERROR || physicalAddressLength < 6)
    {
        Utilities::setLastError("Error getting mac address of tv");
        return false;
    }

    BYTE* bMacAddr = (BYTE*)&macAddress;
    for (int i = 0; i < (int)physicalAddressLength; i++)
    {
        mac[i] = (char)bMacAddr[i];
    }

    for (int i = 0; i < 16; ++i)
        MacBuffer_.append(mac);
    return true;

}