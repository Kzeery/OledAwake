#pragma once
#include "TVCommunicationRuntime.h"
#include "Runtime.h"

enum class RemoteRequest
{
    GET_MONITOR_STATE = 1,
    QUIT              = 2
};

class ClientServerRuntime : public Runtime
{
public:

    bool init();

    MonitorState getOtherMonitorState() const;
    void setCurrentMonitorState(MonitorState state);
    MonitorState getCurrentMonitorState();
    bool isDeviceManager() const { return IsDeviceManager_; };
    bool getTimeSinceLastUserInput(ULONGLONG&) const;
    bool postWindowsTurnOffDisplayMessage() const;
private:
    ClientServerRuntime() : LocalIP_(""), RemotePipeName_(L""), State_(MonitorState::MONITOR_ON) {}
    ~ClientServerRuntime();
    bool ensureServerEnvironment();
    bool initServer();
    bool HandleConnection(HANDLE& hPipe);

private:
    static Runtime* getInstance();

    std::unique_ptr<std::thread> PipeServerThread_;
    std::string LocalIP_;
    std::wstring RemotePipeName_;
    MonitorState State_ = MonitorState::MONITOR_ON;
    static std::unique_ptr<Runtime> Instance_;
    bool IsDeviceManager_ = false;
    friend class RuntimeManager;
};



