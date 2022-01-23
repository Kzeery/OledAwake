// OledAwakeApp.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "PipeCommunication.h"
DWORD WINAPI InstanceThread(LPVOID);
#define IN_BUFFER_SIZE sizeof(Request)
#define OUT_BUFFER_SIZE sizeof(ULONGLONG)
typedef DWORD(WINAPI* pGetLastInputTime)(BOOL*);
pGetLastInputTime getLastInputTime;
HHOOK hMouseHook;
HHOOK hKeyboardHook;
HINSTANCE hDLL = NULL;
HANDLE successEvent = NULL;
bool exitNow = false;
void MessageLoop()
{
    MSG message;
    while (GetMessage(&message, NULL, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
}

DWORD WINAPI executeHookThread(LPVOID lpParm)
{
    hDLL = LoadLibrary(L"OledAwakeHook.dll");
    if (!hDLL) return 1;
    getLastInputTime = (pGetLastInputTime)GetProcAddress(hDLL, "getLastInputTime");
    if (!getLastInputTime) return -1;
    hMouseHook = SetWindowsHookEx(WH_MOUSE_LL, (HOOKPROC)GetProcAddress(hDLL, "MouseProc"), hDLL, NULL);
    hKeyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)GetProcAddress(hDLL, "KBProc"), hDLL, NULL);
    if (hMouseHook == NULL || hKeyboardHook == NULL)
    {
        return 2;
    }
    SetEvent(successEvent);
    MessageLoop();
    UnhookWindowsHookEx(hMouseHook);
    UnhookWindowsHookEx(hMouseHook);
    FreeLibrary(hDLL);
    hDLL = NULL;
    getLastInputTime = NULL;
    exitNow = true;
    return 0;
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                     _In_opt_ HINSTANCE hPrevInstance,
                     _In_ LPWSTR    lpCmdLine,
                     _In_ int       nCmdShow)
{
    UNREFERENCED_PARAMETER(hInstance);
    UNREFERENCED_PARAMETER(hPrevInstance);
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

    HANDLE instanceMutex = CreateMutex(NULL, TRUE, L"OledAwakeAppMutex");
    if (instanceMutex == NULL || GetLastError() == ERROR_ALREADY_EXISTS) {
        return 0;
    }

    successEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
    if (!successEvent) return 3;
    HANDLE hHookThread = NULL;
    DWORD dwThread;
    hHookThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)executeHookThread, NULL, NULL, &dwThread);
    if (!hHookThread) return -1;
    HANDLE events[2] = { successEvent, hHookThread };
    DWORD waitResult = WaitForMultipleObjects(2, events, FALSE, INFINITE) - WAIT_OBJECT_0;
    CloseHandle(successEvent);
    successEvent = NULL;
    if (waitResult != 0)
    {
        // thread exited, cleanup
        CloseHandle(hHookThread);
        return 2;
    }
    HANDLE hPipe = INVALID_HANDLE_VALUE, hThread = NULL;
    DWORD  dwThreadId = 0;
    while (true)
    {
        if (hThread != NULL)
        {
            WaitForSingleObject(hThread, INFINITE);
            hThread = NULL;
        }
        if (exitNow) return 4;

        hPipe = CreateNamedPipe(PIPE_NAME, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_WAIT, PIPE_UNLIMITED_INSTANCES, OUT_BUFFER_SIZE, IN_BUFFER_SIZE, NULL, NULL);
        if (hPipe != INVALID_HANDLE_VALUE)
        {
            BOOL fConnected = ConnectNamedPipe(hPipe, NULL) ?
                TRUE : (GetLastError() == ERROR_PIPE_CONNECTED);
            if (fConnected)
            {
                hThread = CreateThread(
                    NULL,              // no security attribute 
                    0,                 // default stack size 
                    InstanceThread,    // thread proc
                    (LPVOID)hPipe,    // thread parameter 
                    0,                 // not suspended 
                    &dwThreadId);

                if (hThread == NULL)
                    return -1;
                else
                    CloseHandle(hThread);
            }
            else
                CloseHandle(hPipe);
        }
    }

    return 1;


}


DWORD WINAPI InstanceThread(LPVOID lpvParam)
// This routine is a thread processing function to read from and reply to a client
// via the open pipe connection passed from the main loop. Note this allows
// the main loop to continue executing, potentially creating more threads of
// of this procedure to run concurrently, depending on the number of incoming
// client connections.
{
    HANDLE hHeap = GetProcessHeap();
    Request* pchRequest = (Request*)HeapAlloc(hHeap, 0, IN_BUFFER_SIZE);
    ULONGLONG* pchReply = (ULONGLONG*)HeapAlloc(hHeap, 0, OUT_BUFFER_SIZE);

    DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
    BOOL fSuccess = FALSE;
    HANDLE hPipe = NULL;

    // Do some extra error checking since the app will keep running even if this
    // thread fails.

    if (lpvParam == NULL)
    {
        if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
        if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
        return (DWORD)-1;
    }

    if (pchRequest == NULL)
    {
        if (pchReply != NULL) HeapFree(hHeap, 0, pchReply);
        return (DWORD)-1;
    }

    if (pchReply == NULL)
    {
        if (pchRequest != NULL) HeapFree(hHeap, 0, pchRequest);
        return (DWORD)-1;
    }

    hPipe = (HANDLE)lpvParam;

    // Loop until done reading
    while (1)
    {
        // Read client requests from the pipe. This simplistic code only allows messages
        // up to BUFSIZE characters in length.
        fSuccess = ReadFile(
            hPipe,        // handle to pipe 
            pchRequest,    // buffer to receive data 
            IN_BUFFER_SIZE, // size of buffer 
            &cbBytesRead, // number of bytes read 
            NULL);        // not overlapped I/O 

        if (!fSuccess || cbBytesRead == 0)
            break;
        switch (*pchRequest)
        {
        case (Request::GET_LAST_INPUT_TIME_DWORD):
        {
            BOOL mouse;
            *pchReply = getLastInputTime ? static_cast<ULONGLONG>(getLastInputTime(&mouse)) : 0;
            break;
        }
        case (Request::TURN_OFF_DISPLAY):
            *pchReply = static_cast<ULONGLONG>(PostMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2));
            break;
        default:
            *pchReply = 0;
            break;

        }
        // Process the incoming message.

        // Write the reply to the pipe. 
        fSuccess = WriteFile(
            hPipe,        // handle to pipe 
            pchReply,     // buffer to write from 
            OUT_BUFFER_SIZE, // number of bytes to write 
            &cbWritten,   // number of bytes written 
            NULL);        // not overlapped I/O 

        if (!fSuccess || OUT_BUFFER_SIZE != cbWritten)
            break;
    }

    // Flush the pipe to allow the client to read the pipe's contents 
    // before disconnecting. Then disconnect the pipe, and close the 
    // handle to this pipe instance. 

    FlushFileBuffers(hPipe);
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);

    HeapFree(hHeap, 0, pchRequest);
    HeapFree(hHeap, 0, pchReply);

    return 1;
}
