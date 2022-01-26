#pragma once
#include <SDKDDKVer.h>
#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define EVENT_NAMES_SIZE 5
typedef unsigned long long ULL;
enum
{
    MONITOR_ON_EVENT_INDEX = 0,
    MONITOR_OFF_EVENT_INDEX = 1,
    SWITCH_HDMI1_EVENT_INDEX = 2,
    SWITCH_HDMI2_EVENT_INDEX = 3,
    SERVER_EXITED_EVENT_INDEX = 4,
    EXIT_SERVICE_EVENT_INDEX = 5
};

class Utilities
{
public:

    [[nodiscard]] static std::string narrow(std::wstring sInput);
    [[nodiscard]] static std::wstring widen(std::string& str);
    [[nodiscard]] static std::wstring getLastError();
    [[nodiscard]] static ULL absoluteDiff(const ULL& a, const ULL& b);
    static void setLastError(std::string error);
    static wchar_t* eventNames[EVENT_NAMES_SIZE];

    
private:
    static std::string LastError_;

};