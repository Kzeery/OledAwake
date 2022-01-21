#pragma once
#include "TVCommunicationRuntime.h"
#include "Client.h"
#include "Server.h"
#include "Runtime.h"
class ClientServerRuntime : public Runtime
{
public:

    bool init();
    static Runtime* getInstance();
    MonitorState getOtherMonitorState() const;
    void setCurrentMonitorState(MonitorState state);
    MonitorState getCurrentMonitorState();
    bool cleanUpAndReconnect();
private:
    ClientServerRuntime() : ClientServerObject_(nullptr), LocalIP_(""), State_(MonitorState::MONITOR_ON) {}
    ~ClientServerRuntime();
    bool ensureServerEnvironment();
    bool initServer();
    bool initClient();

    void sendMessageToClients(MonitorState& state);
private:
    std::unique_ptr<ReadableClientServerObject> ClientServerObject_;
    std::unique_ptr<std::thread> ClientServerObjectThread_;
    std::string LocalIP_;
    MonitorState State_ = MonitorState::MONITOR_ON;
    static std::unique_ptr<Runtime> Instance_;

};



