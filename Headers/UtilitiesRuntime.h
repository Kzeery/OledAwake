#pragma once
#include <SDKDDKVer.h>
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <string>
#include <Windows.h>
#include <fstream>
#include <WS2tcpip.h>
#include <WinSock2.h>
#include "ChatServer.h"
#include "ChatClient.h"
#pragma comment(lib, "Ws2_32.lib")
#ifdef __has_include
    #if __has_include("Environment.h")
        #include "Environment.h"
    #endif
#endif

#define HANDSHAKE_NOTPAIRED		L"{\"type\":\"register\",\"id\":\"register_0\",\"payload\":{\"forcePairing\":false,\"pairingType\":\"PROMPT\",\"manifest\":{\"manifestVersion\":1,\"appVersion\":\"1.1\",\"signed\":{\"created\":\"20140509\",\"appId\":\"com.lge.test\",\"vendorId\":\"com.lge\",\"localizedAppNames\":{\"\":\"LG Remote App\",\"ko-KR\":\"리모컨 앱\",\"zxx-XX\":\"ЛГ Rэмotэ AПП\"},\"localizedVendorNames\":{\"\":\"LG Electronics\"},\"permissions\":[\"TEST_SECURE\",\"CONTROL_INPUT_TEXT\",\"CONTROL_MOUSE_AND_KEYBOARD\",\"READ_INSTALLED_APPS\",\"READ_LGE_SDX\",\"READ_NOTIFICATIONS\",\"SEARCH\",\"WRITE_SETTINGS\",\"WRITE_NOTIFICATION_ALERT\",\"CONTROL_POWER\",\"READ_CURRENT_CHANNEL\",\"READ_RUNNING_APPS\",\"READ_UPDATE_INFO\",\"UPDATE_FROM_REMOTE_APP\",\"READ_LGE_TV_INPUT_EVENTS\",\"READ_TV_CURRENT_TIME\"],\"serial\":\"2f930e2d2cfe083771f68e4fe7bb07\"},\"permissions\":[\"LAUNCH\",\"LAUNCH_WEBAPP\",\"APP_TO_APP\",\"CLOSE\",\"TEST_OPEN\",\"TEST_PROTECTED\",\"CONTROL_AUDIO\",\"CONTROL_DISPLAY\",\"CONTROL_INPUT_JOYSTICK\",\"CONTROL_INPUT_MEDIA_RECORDING\",\"CONTROL_INPUT_MEDIA_PLAYBACK\",\"CONTROL_INPUT_TV\",\"CONTROL_POWER\",\"READ_APP_STATUS\",\"READ_CURRENT_CHANNEL\",\"READ_INPUT_DEVICE_LIST\",\"READ_NETWORK_STATE\",\"READ_RUNNING_APPS\",\"READ_TV_CHANNEL_LIST\",\"WRITE_NOTIFICATION_TOAST\",\"READ_POWER_STATE\",\"READ_COUNTRY_INFO\"],\"signatures\":[{\"signatureVersion\":1,\"signature\":\"eyJhbGdvcml0aG0iOiJSU0EtU0hBMjU2Iiwia2V5SWQiOiJ0ZXN0LXNpZ25pbmctY2VydCIsInNpZ25hdHVyZVZlcnNpb24iOjF9.hrVRgjCwXVvE2OOSpDZ58hR+59aFNwYDyjQgKk3auukd7pcegmE2CzPCa0bJ0ZsRAcKkCTJrWo5iDzNhMBWRyaMOv5zWSrthlf7G128qvIlpMT0YNY+n/FaOHE73uLrS/g7swl3/qH/BGFG2Hu4RlL48eb3lLKqTt2xKHdCs6Cd4RMfJPYnzgvI4BNrFUKsjkcu+WD4OO2A27Pq1n50cMchmcaXadJhGrOqH5YmHdOCj5NSHzJYrsW0HPlpuAx/ECMeIZYDh6RMqaFM2DXzdKX9NmmyqzJ3o/0lkk/N97gfVRLW5hA29yeAwaCViZNCP8iC9aO0q9fQojoa7NQnAtw==\"}]}}}"
#define HANDSHAKE_PAIRED		L"{\"type\":\"register\",\"id\":\"register_0\",\"payload\":{\"forcePairing\":false,\"pairingType\":\"PROMPT\",\"client-key\":\"CLIENTKEYx0x0x0\",\"manifest\":{\"manifestVersion\":1,\"appVersion\":\"1.1\",\"signed\":{\"created\":\"20140509\",\"appId\":\"com.lge.test\",\"vendorId\":\"com.lge\",\"localizedAppNames\":{\"\":\"LG Remote App\",\"ko-KR\":\"리모컨 앱\",\"zxx-XX\":\"ЛГ Rэмotэ AПП\"},\"localizedVendorNames\":{\"\":\"LG Electronics\"},\"permissions\":[\"TEST_SECURE\",\"CONTROL_INPUT_TEXT\",\"CONTROL_MOUSE_AND_KEYBOARD\",\"READ_INSTALLED_APPS\",\"READ_LGE_SDX\",\"READ_NOTIFICATIONS\",\"SEARCH\",\"WRITE_SETTINGS\",\"WRITE_NOTIFICATION_ALERT\",\"CONTROL_POWER\",\"READ_CURRENT_CHANNEL\",\"READ_RUNNING_APPS\",\"READ_UPDATE_INFO\",\"UPDATE_FROM_REMOTE_APP\",\"READ_LGE_TV_INPUT_EVENTS\",\"READ_TV_CURRENT_TIME\"],\"serial\":\"2f930e2d2cfe083771f68e4fe7bb07\"},\"permissions\":[\"LAUNCH\",\"LAUNCH_WEBAPP\",\"APP_TO_APP\",\"CLOSE\",\"TEST_OPEN\",\"TEST_PROTECTED\",\"CONTROL_AUDIO\",\"CONTROL_DISPLAY\",\"CONTROL_INPUT_JOYSTICK\",\"CONTROL_INPUT_MEDIA_RECORDING\",\"CONTROL_INPUT_MEDIA_PLAYBACK\",\"CONTROL_INPUT_TV\",\"CONTROL_POWER\",\"READ_APP_STATUS\",\"READ_CURRENT_CHANNEL\",\"READ_INPUT_DEVICE_LIST\",\"READ_NETWORK_STATE\",\"READ_RUNNING_APPS\",\"READ_TV_CHANNEL_LIST\",\"WRITE_NOTIFICATION_TOAST\",\"READ_POWER_STATE\",\"READ_COUNTRY_INFO\"],\"signatures\":[{\"signatureVersion\":1,\"signature\":\"eyJhbGdvcml0aG0iOiJSU0EtU0hBMjU2Iiwia2V5SWQiOiJ0ZXN0LXNpZ25pbmctY2VydCIsInNpZ25hdHVyZVZlcnNpb24iOjF9.hrVRgjCwXVvE2OOSpDZ58hR+59aFNwYDyjQgKk3auukd7pcegmE2CzPCa0bJ0ZsRAcKkCTJrWo5iDzNhMBWRyaMOv5zWSrthlf7G128qvIlpMT0YNY+n/FaOHE73uLrS/g7swl3/qH/BGFG2Hu4RlL48eb3lLKqTt2xKHdCs6Cd4RMfJPYnzgvI4BNrFUKsjkcu+WD4OO2A27Pq1n50cMchmcaXadJhGrOqH5YmHdOCj5NSHzJYrsW0HPlpuAx/ECMeIZYDh6RMqaFM2DXzdKX9NmmyqzJ3o/0lkk/N97gfVRLW5hA29yeAwaCViZNCP8iC9aO0q9fQojoa7NQnAtw==\"}]}}}"
#define SERVICE_PORT             "3000"
#define OLED_TV_PORT 9
enum class MonitorState {
    MONITOR_OFF = 0,
    MONITOR_ON = 1
};
#ifndef OLED_ENVIRONMENT
    #define OLED_TV_IP "255.255.255.255"
    #define OLED_MAC_ADDRESS "\x00\x00\x00\x00\x00\x00"
    #define SERVER_IP_ADDRESS "0.0.0.0"
#endif

//Error codes
#define SETUP_SESSION_KEY_ERROR "Failed to set up session key with Oled TV. Please check config. Error Message: "
#define TURN_OFF_DISPLAY_ERROR "Failed to turn off display. Encountered an error. Please check config. Error Message: "
#define TOO_MUCH_TIME_ERROR "Exiting Operation, taking too long."
#define ESCAPE_TOO_MUCH_TIME(o, max) if(time(0) - o > max) { Last_error_ = TOO_MUCH_TIME_ERROR; return false; }
typedef BOOL(_cdecl* PATH_APPEND_TYPE)(LPWSTR, LPCWSTR);
using namespace std;

class UtilitiesRuntime
{
public:
    UtilitiesRuntime(bool &result);
    ~UtilitiesRuntime();
    [[nodiscard]] string narrow(wstring sInput) const;
    bool turnOffDisplay() const;
    [[nodiscard]] wstring getLastError() const;
    bool turnOnDisplay() const;
    [[nodiscard]] wstring widen(string& str) const;
    MonitorState getCurrentMonitorState() const { return State_; };
    void setCurrentMonitorState(MonitorState state) { State_ = state; sendMessageToClients(state); };
    static unsigned __stdcall initChatServer(void *pUserData);
    bool connectClient();
    bool ensureServerEnvironment();
    void sendMessageToClients(MonitorState state);
    MonitorState getOtherMonitorState() const;
    ChatClient* Client_ = nullptr;
private:

    bool init();
    bool loadKeyFromFile();
    bool setupSessionKey();
    bool createWakeupEvent() const;
    void initWOL();
    void initHandshake();
    void saveKeyToFile() const;

    static HANDLE monitorOnEvent;
    static HANDLE monitorOffEvent;

    string Key_;
    string Handshake_;

    static string LocalIP_;
    static string Last_error_;
    static string Path_to_KeyFile_;
    static string MacBuffer_;


    const string Ck_ = "CLIENTKEYx0x0x0";
    const string Host_ = "192.168.1.234";
    const string Poweroffmess_ = "{ \"id\":\"0\",\"type\" : \"request\",\"uri\" : \"ssap://system/turnOff\"}";

    MonitorState State_ = MonitorState::MONITOR_ON;
};




