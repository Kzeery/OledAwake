#include "TVCommunicationRuntime.h"
#include <shlobj_core.h>
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = boost::asio::ip::tcp;

std::unique_ptr<Runtime> TVCommunicationRuntime::Instance_ = nullptr;
bool TVCommunicationRuntime::InitializedOnce_ = false;
Runtime* TVCommunicationRuntime::getInstance(bool newInstance)
{
    if (Instance_.get() == nullptr && !InitializedOnce_ && newInstance)
    {
        Instance_ = std::make_unique<TVCommunicationRuntime>(TVCommunicationRuntime::TVCToken{});
    }
    InitializedOnce_ = true;
    return Instance_.get();
}
bool TVCommunicationRuntime::init()
{
    ensureTVMacAddress();
    Object<HINSTANCE> hDLL = LoadLibrary(L"Shlwapi.dll");
    if (hDLL == NULL)
        SET_ERROR_EXIT("Failed to load Shlwapi.dll", false);

    PATH_APPEND_TYPE pathAppend = (PATH_APPEND_TYPE)GetProcAddress(hDLL, "PathAppendA");
    if (pathAppend == NULL)
    {
        Utilities::setLastError("Failed to load PathAppendA function");
        return false;
    }
    CHAR szPath[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_COMMON_APPDATA, NULL, 0, szPath)))
    {
        pathAppend(szPath, "OledAwake");
        CreateDirectoryA(szPath, NULL);
        pathAppend(szPath, "KeyFile.txt");
        Path_to_KeyFile_ = szPath;
    }

    if (Path_to_KeyFile_.empty())
        SET_ERROR_EXIT("Failed to get KeyFile path", false);

    if (!loadKeyFromFile())
    {
        if (!setupSessionKey())
            SET_ERROR_EXIT("Failed to setup session key", false);
        if (Key_.empty())
            SET_ERROR_EXIT("Received invalid session key", false)
        Handshake_ = Utilities::narrow(PairedHandshake);
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
            Handshake_ = Utilities::narrow(PairedHandshake);
            keyFile.close();
            return true;
        }
        keyFile.close();
    }
    Key_ = "";
    Handshake_ = Utilities::narrow(UnpairedHandshake);
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
    size_t ckf = Handshake_.find(HandshakeIdentifier);
    Handshake_.replace(ckf, strlen(HandshakeIdentifier), Key_);
}

bool TVCommunicationRuntime::setupSessionKey()
{
    std::string host = OledTVIP;

    try
    {
        net::io_context ioc;
        tcp::resolver resolver{ ioc };
        websocket::stream<tcp::socket> ws{ ioc };
        beast::flat_buffer buffer;
        auto const results = resolver.resolve(host, ServicePort);
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

bool TVCommunicationRuntime::turnOnDisplay()
{
    ensureTVMacAddress();
    
    int iResult;
    const char* SendBuf = MacBuffer_.c_str();
    int BufLen = 102;

    sockaddr_in RecvAddr;
    Object<SOCKET> SendSocket;
    SendSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (SendSocket == INVALID_SOCKET) {
        Utilities::setLastError("Failed to initialize socket");
        return false;
    }
    BOOL val = true;
    if (setsockopt(SendSocket, SOL_SOCKET, SO_BROADCAST, (const char*)&val, sizeof(BOOL)) != 0)
    {
        Utilities::setLastError("Failed to set socket options");
        return false;
    }

    RecvAddr.sin_family = AF_INET;
    RecvAddr.sin_port = htons(OledTVPort);
    iResult = InetPtonA(AF_INET, BroadcastAddress, &RecvAddr.sin_addr.s_addr);
    if (iResult != 1)
    {
        Utilities::setLastError("Failed to convert IP string to address");
        return false;
    }

    iResult = sendto(SendSocket,
        SendBuf, BufLen, 0, (SOCKADDR*)&RecvAddr, sizeof(RecvAddr));
    if (iResult == SOCKET_ERROR) {
        Utilities::setLastError("Failed to send socket");
        return false;
    }

    return true;
}
bool TVCommunicationRuntime::sendMessageToTV(const char* input) const
{
    try
    {
        std::string host = OledTVIP;
        time_t origtim = time(0);
        net::io_context ioc;

        tcp::resolver resolver{ ioc };
        websocket::stream<tcp::socket> ws{ ioc };
        auto const results = resolver.resolve(host, ServicePort);
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
        snprintf(buffer, 400, "Failed to send message to tv.\nMessage sent: (%s)\nError Message: (%s)", input, e.what());
        SET_ERROR_EXIT(buffer, false);
    }
    return true;
}

bool TVCommunicationRuntime::switchInput(Inputs input) const
{
    const char* message =  input == Inputs::HDMI1 ? SWITCH_INPUT_MESSAGE(HDMI_1) : SWITCH_INPUT_MESSAGE(HDMI_2);
    return sendMessageToTV(message);
}

bool TVCommunicationRuntime::turnOffDisplay() const
{
    return sendMessageToTV(TurnOffTVMessage);
}

bool TVCommunicationRuntime::ensureTVMacAddress()
{
    if (MacBuffer_.size() > 6) return true;

    char mac[6]{ 0 };
    in_addr destIP{ 0 };
    in_addr srcIP{ 0 };
    srcIP.s_addr = INADDR_ANY;

    ULONG macAddress[2]{ 0 };
    ULONG physicalAddressLength = 6;


    if (WSAStartup(MAKEWORD(2, 2), &WSAData_) != 0)
    {
        Utilities::setLastError("Error starting WSA");
        return false;	//Return 1 on error
    }
    WSASuccessful_ = true;
    Object<HINSTANCE> hDLL(LoadLibrary(L"iphlpapi.dll"));
    if (!hDLL)
    {
        Utilities::setLastError("Error loading iphlapi library");
        return false;
    }
    SendArpType SendArp = (SendArpType)GetProcAddress(hDLL, "SendARP");

    if (SendArp == NULL)
    {
        Utilities::setLastError("Error getting process address of GetArp function");
        return false;
    }

    int iResult = InetPtonA(AF_INET, OledTVIP, &destIP.s_addr);
    if (iResult != 1)
    {
        LPSTR buff{ 0 };
        DWORD dw = GetLastError();
        FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER |
            FORMAT_MESSAGE_FROM_SYSTEM |
            FORMAT_MESSAGE_IGNORE_INSERTS,
            NULL,
            dw,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            (LPSTR)&buff,
            0, NULL);
        Utilities::setLastError(buff);
        LocalFree(buff);
        return false;
    }

    DWORD ret = SendArp(destIP, srcIP, macAddress, &physicalAddressLength);


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