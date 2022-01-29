#pragma once
#include "Utilities.h"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <fstream>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include "Runtime.h"
#include "Environment.h"

#pragma comment(lib, "Ws2_32.lib")
//Error escapes
#define ESCAPE_TOO_MUCH_TIME(o, max) if(time(0) - o > max) { Utilities::setLastError("Exiting Operation, taking too long."); return false; }
#define SET_ERROR_EXIT(msg, retval) { Utilities::setLastError(msg); return retval; }

enum class MonitorState {
    MONITOR_OFF = 0,
    MONITOR_ON  = 1,
    UNKNOWN     = 2
};

enum class Inputs {
    HDMI1 = 1,
    HDMI2 = 2
};

typedef BOOL(_cdecl* PATH_APPEND_TYPE)(LPSTR, LPCSTR);
typedef DWORD(_cdecl* SendArpType)(IN_ADDR, IN_ADDR, PVOID, PULONG);

class TVCommunicationRuntime : public Runtime
{
public:
    bool turnOffDisplay() const;
    bool turnOnDisplay();
    bool switchInput(Inputs) const;
    static Runtime* getInstance(bool);
private:
    TVCommunicationRuntime(): WSAData_(WSADATA()) {};
    ~TVCommunicationRuntime() {  };
    bool loadKeyFromFile();
    bool setupSessionKey();
    void initHandshake();
    void saveKeyToFile() const;
    bool ensureTVMacAddress();
    bool sendMessageToTV(const char*) const;

    bool init();
    std::unique_ptr<Runtime>& destroy() { if(WSASuccessful_) WSACleanup(); return Instance_; }
    std::string Key_;
    std::string Handshake_;

    std::string Path_to_KeyFile_ = "";
    std::string MacBuffer_ = "\xff\xff\xff\xff\xff\xff";
    WSADATA WSAData_;
    bool WSASuccessful_{ false };
    static bool InitializedOnce_;
    static std::unique_ptr<Runtime> Instance_;
};
