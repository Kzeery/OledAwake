#pragma once
#include "TVCommunicationRuntime.h"
#include "Runtime.h"

enum class RemoteRequest
{
    REQUEST_UNKNOWN   = 0,
    GET_MONITOR_STATE = 1,
    QUIT              = 2
};
struct SendServerInfoStruct
{
    RemoteRequest req;
    MonitorState* res;
    std::string ip;
};
class ClientServerRuntime : public Runtime
{
public:
    bool init();

    MonitorState getOtherMonitorState() const;
    void setCurrentMonitorState(MonitorState);
    MonitorState getCurrentMonitorState();
    bool isDeviceManager() const { return IsDeviceManager_; };
    bool getTimeSinceLastUserInput(ULONGLONG&) const;
    bool postWindowsTurnOffDisplayMessage() const;
    void sendMessageToServerTimeout(RemoteRequest, MonitorState*, bool, int) const;
private:
    ClientServerRuntime() : LocalIP_(""), OtherIP_(""), State_(MonitorState::MONITOR_ON) {}
    ~ClientServerRuntime();
    bool ensureServerEnvironment();
    bool initServer();

private:
    static Runtime* getInstance();

    std::unique_ptr<std::thread> ServerThread_;
    std::string LocalIP_;
    std::string OtherIP_;
    MonitorState State_ = MonitorState::MONITOR_ON;
    static std::unique_ptr<Runtime> Instance_;
    bool IsDeviceManager_ = false;
    friend class RuntimeManager;

};



