#include "pch.h"
DWORD lastInputTime = 0;
std::mutex myMutex;
#ifdef __cplusplus
extern "C"
{
#endif
    __declspec(dllexport) LRESULT CALLBACK MouseProc(
        int nCode,
        WPARAM wParam,
        LPARAM lParam
    )
    {
        myMutex.lock();
        PMSLLHOOKSTRUCT mouseStruct = (PMSLLHOOKSTRUCT)lParam;
        if (mouseStruct->time > lastInputTime)
            lastInputTime = mouseStruct->time;
        myMutex.unlock();
        // process event

        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    __declspec(dllexport) LRESULT CALLBACK KBProc(
        int nCode,
        WPARAM wParam,
        LPARAM lParam
    )
    {
        myMutex.lock();
        PKBDLLHOOKSTRUCT keyboardStruct = (PKBDLLHOOKSTRUCT)lParam;
        if (keyboardStruct->time > lastInputTime)
            lastInputTime = keyboardStruct->time;
        myMutex.unlock();
        // process event

        return CallNextHookEx(NULL, nCode, wParam, lParam);
    }

    __declspec(dllexport) DWORD WINAPI getLastInputTime()
    {
        return lastInputTime;
    }


#ifdef __cplusplus
}
#endif
BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

