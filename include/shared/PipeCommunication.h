#pragma once
#define PIPE_NAME L"\\\\.\\pipe\\GetLastInputTimePipe"
enum class Request
{
    GET_LAST_INPUT_TIME_DWORD = 1,
    TURN_OFF_DISPLAY = 2
};