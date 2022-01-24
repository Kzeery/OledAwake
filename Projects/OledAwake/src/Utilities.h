#pragma once
#include <SDKDDKVer.h>
#include <string>
#include <vector>
enum
{
    MONITOR_ON_EVENT_INDEX = 0,
    MONITOR_OFF_EVENT_INDEX = 1,
    SWITCH_HDMI1_EVENT_INDEX = 2,
    SWITCH_HDMI2_EVENT_INDEX = 3,
    CLIENT_SERVER_EXITED_EVENT_INDEX = 4,
    KILL_MONITOR_THREAD_INDEX = 5
};

class Utilities
{
public:

    [[nodiscard]] static std::string narrow(std::wstring sInput);
    [[nodiscard]] static std::wstring widen(std::string& str);
    [[nodiscard]] static std::wstring getLastError();

    static void setLastError(std::string error);
    static std::vector<wchar_t*> eventNames;

    
private:
    static std::string LastError_;

};