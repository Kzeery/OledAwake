#pragma once
#include <SDKDDKVer.h>
#include <string>
class Utilities
{
public:

    [[nodiscard]] static std::string narrow(std::wstring sInput);
    [[nodiscard]] static std::wstring widen(std::string& str);
    [[nodiscard]] static std::wstring getLastError();
    static void setLastError(std::string error);
private:
    static std::string LastError_;

};