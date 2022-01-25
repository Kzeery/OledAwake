// OledAwakeApp.cpp : Defines the entry point for the application.
//

#include "framework.h"
#include "PipeCommunication.h"
#define IN_BUFFER_SIZE sizeof(Request)
#define OUT_BUFFER_SIZE sizeof(ULONGLONG)
DWORD WINAPI        InstanceThread(LPVOID);
DWORD WINAPI        PipeThread(LPVOID);
void                cleanup(HANDLE&);
ATOM                registerOledClass(HINSTANCE);
BOOL                initInstance(HINSTANCE, int);
LRESULT CALLBACK    WndProc(HWND, UINT, WPARAM, LPARAM);
BOOL                MessageLoop();

HINSTANCE hInst;                                // current instance
bool exitNow = false;
ULONGLONG lastInputTime = 0;

const WCHAR* szTitle = L"OledAwakeApp";                  // The title bar text
const WCHAR* szWindowClass = L"OledAwakeClass";


DWORD WINAPI PipeThread(LPVOID)
{
    HANDLE hPipe = INVALID_HANDLE_VALUE;
    HANDLE hThread = NULL;
    DWORD  dwThreadId = 0;
    while (true)
    {
        if (exitNow)
        {
            PostQuitMessage(0);
            return 4;
        }

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
                {
                    PostQuitMessage(0);
                    return 5;
                }
                else
                    CloseHandle(hThread);
            }
            else
                CloseHandle(hPipe);
        }
    }
    PostQuitMessage(0);
    return 1;
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
        return FALSE;
    }

    registerOledClass(hInstance);
    if (!initInstance(hInstance, nCmdShow))
    {
        return FALSE;
    }
    HANDLE hPipeThread;
    hPipeThread = CreateThread(NULL, 0, PipeThread, NULL, 0, NULL);
    if (hPipeThread == NULL)
    {
        return FALSE;
    }
    MessageLoop();
    WaitForSingleObject(hPipeThread, 3000);
    CloseHandle(hPipeThread);
    return TRUE;
}


ATOM registerOledClass(HINSTANCE hInstance)
{
    WNDCLASSEXW wcex;

    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = NULL;
    wcex.hCursor = NULL;
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszMenuName = MAKEINTRESOURCEW(IDC_OLEDAWAKEAPP);
    wcex.lpszClassName = szWindowClass;
    wcex.hIconSm = NULL;

    return RegisterClassExW(&wcex);
}

BOOL initInstance(HINSTANCE hInstance, int nCmdShow)
{
    hInst = hInstance; // Store instance handle in our global variable

    HWND hWnd = CreateWindowW(szWindowClass, szTitle, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, hInstance, nullptr);

    if (!hWnd)
    {
        return FALSE;
    }


    RAWINPUTDEVICE Rid[2];

    Rid[0].usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
    Rid[0].usUsage = 0x02;              // HID_USAGE_GENERIC_MOUSE
    Rid[0].dwFlags = RIDEV_INPUTSINK;    // adds mouse and also ignores legacy mouse messages
    Rid[0].hwndTarget = hWnd;

    Rid[1].usUsagePage = 0x01;          // HID_USAGE_PAGE_GENERIC
    Rid[1].usUsage = 0x06;              // HID_USAGE_GENERIC_KEYBOARD
    Rid[1].dwFlags = RIDEV_INPUTSINK;    // adds keyboard and also ignores legacy keyboard messages
    Rid[1].hwndTarget = hWnd;

    if (RegisterRawInputDevices(Rid, 2, sizeof(Rid[0])) == FALSE)
    {
        return FALSE;
    }
    UpdateWindow(hWnd);

    return TRUE;
}

DWORD WINAPI InstanceThread(LPVOID lpvParam)
{
    std::unique_ptr<Request> pchRequest(new Request);
    std::unique_ptr<ULONGLONG> pchReply(new ULONGLONG(0));

    DWORD cbBytesRead = 0, cbReplyBytes = 0, cbWritten = 0;
    BOOL fSuccess = FALSE;
    HANDLE hPipe = NULL;

    // Do some extra error checking since the app will keep running even if this
    // thread fails.

    if (lpvParam == NULL)
    {
        return (DWORD)-1;
    }

    hPipe = (HANDLE)lpvParam;

    fSuccess = ReadFile(
        hPipe,        // handle to pipe 
        pchRequest.get(),    // buffer to receive data 
        IN_BUFFER_SIZE, // size of buffer 
        &cbBytesRead, // number of bytes read 
        NULL);        // not overlapped I/O 

    if (!fSuccess || cbBytesRead == 0)
    {
        cleanup(hPipe);
        return 2;
    }
    
    DWORD writeSize = sizeof(ULONGLONG);
    switch (*pchRequest)
    {
    case Request::GET_LAST_INPUT_TIME_DWORD:
        *pchReply = lastInputTime;
        break;
    case Request::TURN_OFF_DISPLAY:
        *pchReply = PostMessage(HWND_BROADCAST, WM_SYSCOMMAND, SC_MONITORPOWER, 2) ? ERROR_SUCCESS : GetLastError();
        writeSize = sizeof(DWORD);
        break;
    default:
        cleanup(hPipe);
        return 4;

    }
    // Process the incoming message.

    // Write the reply to the pipe. 
    fSuccess = WriteFile(
        hPipe,        // handle to pipe 
        pchReply.get(),     // buffer to write from 
        writeSize, // number of bytes to write 
        &cbWritten,   // number of bytes written 
        NULL);        // not overlapped I/O 

    if (!fSuccess || writeSize != cbWritten)
    {
        cleanup(hPipe);
        return 3;
    }

    // Flush the pipe to allow the client to read the pipe's contents 
    // before disconnecting. Then disconnect the pipe, and close the 
    // handle to this pipe instance. 
    FlushFileBuffers(hPipe);
    cleanup(hPipe);
    return 1;
}

void WINAPI cleanup(HANDLE& hPipe)
{
    DisconnectNamedPipe(hPipe);
    CloseHandle(hPipe);
}

BOOL MessageLoop()
{
    MSG message;
    while (GetMessage(&message, NULL, 0, 0))
    {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }
    exitNow = true;
    Request req = Request::QUIT_APP;
    DWORD res = 0;
    DWORD bytesRead = 0;
    return CallNamedPipe(PIPE_NAME, &req, sizeof(Request), &res, sizeof(DWORD), &bytesRead, NMPWAIT_NOWAIT);
}

#define WM_INPUT_EXIT() { if (GET_RAWINPUT_CODE_WPARAM(wParam) == RIM_INPUT) return DefWindowProc(hWnd, message, wParam, lParam); break; }
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_INPUT:
    {
        auto t = GetTickCount64();
        if (t - lastInputTime > 1000)
        {
            UINT dwSize = 0;

            GetRawInputData((HRAWINPUT)lParam, RID_INPUT, NULL, &dwSize, sizeof(RAWINPUTHEADER));
            std::unique_ptr<char[]> lpb(new char[dwSize]);

            if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb.get(), &dwSize, sizeof(RAWINPUTHEADER)) != dwSize)
                WM_INPUT_EXIT();

            RAWINPUT* raw = (RAWINPUT*)lpb.get();

            if (raw->header.dwType == RIM_TYPEMOUSE)
            {
                RAWMOUSE mouse = raw->data.mouse;

                if (mouse.usButtonFlags == 0 && mouse.lLastX == 0 && mouse.lLastY == 0 && mouse.ulExtraInformation == 0)
                    WM_INPUT_EXIT();
            }

            lastInputTime = t;
        }
        WM_INPUT_EXIT();
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}