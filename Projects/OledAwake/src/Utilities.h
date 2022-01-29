#pragma once
#include <SDKDDKVer.h>
#include <string>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
enum
{
    UNKNOWN_EVENT_INDEX = -1,
    MONITOR_ON_EVENT_INDEX = 0,
    MONITOR_OFF_EVENT_INDEX = 1,
    SWITCH_HDMI1_EVENT_INDEX = 2,
    SWITCH_HDMI2_EVENT_INDEX = 3,
    SERVER_EXITED_EVENT_INDEX = 4,
    EXIT_SERVICE_EVENT_INDEX = 5
};
constexpr unsigned int eventsSize = EXIT_SERVICE_EVENT_INDEX + 1;

class HandleWrapper
{
public:

    HandleWrapper() : Obj_(nullptr) {};
    HandleWrapper(HANDLE hEvent) : Obj_(hEvent) {};
    ~HandleWrapper() 
    {
        if(Obj_)
            CloseHandle(Obj_);
    };
    operator HANDLE() const { return Obj_; };
    HandleWrapper& operator=(const HANDLE& hEvent) { Obj_ = hEvent; return *this; };

private:
    HANDLE Obj_;    
};

class LibraryWrapper
{
public:

    LibraryWrapper() : Obj_(nullptr) {};
    LibraryWrapper(HINSTANCE hEvent) : Obj_(hEvent) {};
    ~LibraryWrapper()
    {
        if(Obj_)
            FreeLibrary(Obj_);
    };
    operator HINSTANCE() const { return Obj_; };
    LibraryWrapper& operator=(const HINSTANCE& hEvent) { Obj_ = hEvent; return *this; };

private:
    HINSTANCE Obj_;
};

class Utilities
{
public:

    [[nodiscard]] static std::string narrow(std::wstring sInput);
    [[nodiscard]] static std::wstring widen(std::string& str);
    [[nodiscard]] static std::wstring getLastError();
    [[nodiscard]] static ULONGLONG absoluteDiff(const ULONGLONG& a, const ULONGLONG& b);
    static void setLastError(std::string error);
    static HandleWrapper events[eventsSize];

    
private:
    static std::string LastError_;

};