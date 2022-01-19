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
    MONITOR_ON = 1
};

typedef BOOL(_cdecl* PATH_APPEND_TYPE)(LPWSTR, LPCWSTR);
typedef DWORD(_cdecl* SendArpType)(IN_ADDR, IN_ADDR, PVOID, PULONG);

class TVCommunicationRuntime : public Runtime
{
public:
    bool turnOffDisplay() const;
    bool turnOnDisplay() const;
    bool init();
    static Runtime* getInstance();
private:
    TVCommunicationRuntime() {};
    ~TVCommunicationRuntime() {  };
    bool loadKeyFromFile();
    bool setupSessionKey();
    void initHandshake();
    void saveKeyToFile() const;
    bool ensureTVMacAddress();
    bool sendMessageToTV(const char* message) const;
    std::string Key_;
    std::string Handshake_;

    std::string Path_to_KeyFile_ = "";
    std::string MacBuffer_ = "\xff\xff\xff\xff\xff\xff";


    static std::unique_ptr<Runtime> Instance_;

};
