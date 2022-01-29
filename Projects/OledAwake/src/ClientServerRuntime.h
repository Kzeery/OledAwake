#pragma once
#include "TVCommunicationRuntime.h"
#include "Runtime.h"

enum class RemoteRequest
{
    REQUEST_UNKNOWN   = 0,
    GET_MONITOR_STATE = 1,
    SHUTDOWN_SERVER   = 200
};
struct SendServerInfoStruct
{
    RemoteRequest req;
    MonitorState* res;
    std::string ip;
};

constexpr unsigned int RemoteInBufferSize = sizeof(RemoteRequest);
constexpr unsigned int RemoteOutBufferSize = sizeof(MonitorState);

class ClientServerRuntime : public Runtime
{
public:

    MonitorState getOtherMonitorState() const;
    void setCurrentMonitorState(MonitorState);
    MonitorState getCurrentMonitorState();
    bool isDeviceManager() const { return IsDeviceManager_; };
    bool getTimeSinceLastUserInput(ULONGLONG&) const;
    bool postWindowsTurnOffDisplayMessage() const;
    void sendMessageToServerTimeout(RemoteRequest, MonitorState*, bool, int) const;
    static Runtime* getInstance(bool);
private:
    ClientServerRuntime() : LocalIP_(""), OtherIP_(""), State_(MonitorState::MONITOR_ON) {}
    ~ClientServerRuntime() {};
    bool ensureServerEnvironment();
    bool initServer(HandleWrapper&) const;
    bool init();
    std::unique_ptr<Runtime>& destroy();
private:

    std::unique_ptr<std::thread> ServerThread_;
    std::string LocalIP_;
    std::string OtherIP_;
    MonitorState State_ = MonitorState::UNKNOWN;
    static std::unique_ptr<Runtime> Instance_;
    static bool InitializedOnce_;
    bool IsDeviceManager_ = false;

};



