#include "Utilities.h"
#include <Windows.h>
#include <codecvt>
#include <locale>
std::string Utilities::LastError_ = "";
std::vector<wchar_t*> Utilities::eventNames = 
{
    L"MonitorOn",
    L"MonitorOff",
    L"ClientServerExitedThread",
    L"KillMonitorThread"
};
std::wstring Utilities::getLastError()
{
    std::wstring ret = widen(LastError_);
    LastError_ = "";
    return ret;
}
void Utilities::setLastError(std::string error)
{
    LastError_ = move(error);
}


std::wstring Utilities::widen(std::string& str)
{
    return std::wstring_convert<std::codecvt_utf8<wchar_t>>().from_bytes(str);
}


std::string Utilities::narrow(std::wstring sInput) 
{

    // Calculate target buffer size
    long len = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(),
        NULL, 0, NULL, NULL);
    if (len == 0)
        return "";

    // Convert character sequence
    std::string out(len, 0);
    if (len != WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, sInput.c_str(), (int)sInput.size(), &out[0], (int)out.size(), NULL, NULL))
        return "";

    return out;
}

