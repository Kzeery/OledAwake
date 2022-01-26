#pragma once
#define PIPE_NAME L"\\\\.\\pipe\\GetLastInputTimePipe"
enum class Request
{
    GET_TIME_SINCE_LAST_INPUT = 1,
    TURN_OFF_DISPLAY = 2,
    QUIT_APP = 3
};