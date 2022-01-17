#pragma once
#include "Client.h"
#include "Server.h"
#include "UtilitiesRuntime.h"
#include "Runtime.h"
class ClientServerRuntime : public Runtime
{
public:
    ClientServerRuntime() : ClientServerObject_(nullptr), LocalIP_(""), State_(MonitorState::MONITOR_ON) {}
    ~ClientServerRuntime();

    bool init();

    MonitorState getOtherMonitorState() const;
    void setCurrentMonitorState(MonitorState state);
    std::thread& getClientServerObjectThread() { return ClientServerObjectThread_; }
private:
    bool ensureServerEnvironment();
    bool initServer();
    bool initClient();

    void sendMessageToClients(MonitorState& state);

private:
    unique_ptr<ReadableClientServerObject> ClientServerObject_;
    std::thread ClientServerObjectThread_;
    std::string LocalIP_;
    MonitorState State_;
};

