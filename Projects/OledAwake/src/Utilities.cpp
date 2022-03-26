#include "Utilities.h"
#include <Windows.h>
std::string Utilities::LastError_ = "";
Object<HANDLE> Utilities::events[eventsSize]{0};
std::wstring Utilities::getLastError()
{
    std::wstring& ret = widen(LastError_);
    LastError_ = "";
    return ret;
}
void Utilities::setLastError(std::string error)
{
    LastError_ = move(error);
}


std::wstring Utilities::widen(std::string& str)
{
    int len = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), NULL, 0);
    if (len == 0)
        return L"";

    std::wstring out(len, 0);

    if (len != MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, str.c_str(), (int)str.size(), &out[0], (int)out.size()))
        return L"";

    return out;
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