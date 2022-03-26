#pragma once
#include <SDKDDKVer.h>
#include <string>
#include <type_traits>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>
#include <winsock2.h>
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
template <typename T>
class Object
{
public:
    Object(T obj) : Obj_(obj) {}
    Object()  { init(); }
    ~Object() { deleteObj(); }

    void reset()
    {
        deleteObj();
        init();
    }

    operator T() const              { return Obj_; };
    Object& operator=(const T& obj) { Obj_ = obj; return *this; };

    Object& operator=(const Object&) = delete;
    Object(const Object&) = delete;

private:
    void init()
    {
        if constexpr (std::is_same<T, SOCKET>::value)
            Obj_ = INVALID_SOCKET;
        else
            Obj_ = NULL;
    }

    void deleteObj()
    {
        if constexpr (std::is_same<T, HANDLE>::value)
            if (Obj_)
                CloseHandle(Obj_);

        else if constexpr (std::is_same<T, HINSTANCE>::value)
            if (Obj_)
                FreeLibrary(Obj_);

        else if constexpr (std::is_same<T, SOCKET>::value)
            if (Obj_ != INVALID_SOCKET)
                closesocket(Obj_);
    }

private:
    T Obj_;
};

class Utilities
{
public:

    [[nodiscard]] static std::string narrow(std::wstring sInput);
    [[nodiscard]] static std::wstring widen(std::string& str);
    [[nodiscard]] static std::wstring getLastError();
    static void setLastError(std::string error);
    static Object<HANDLE> events[eventsSize];

    
private:
    static std::string LastError_;

};