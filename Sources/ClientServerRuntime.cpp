#include "ClientServerRuntime.h"
ClientServerRuntime::~ClientServerRuntime()
{
    if (ClientServerObject_ != nullptr)
        delete ClientServerObject_;
}

bool ClientServerRuntime::init()
{
    if (ensureServerEnvironment())
    {
        HANDLE serverRunningEvent = CreateEvent(NULL, TRUE, FALSE, L"ServerRunning");
        if (serverRunningEvent == NULL)
        {
            UtilitiesRuntime::setLastError("Could not create Server Running event");
            return false;
        }

        std::thread serverThread(&ClientServerRuntime::initServer, this);
        if (WaitForSingleObject(serverRunningEvent, 5000) == WAIT_TIMEOUT)
        {
            UtilitiesRuntime::setLastError("Server running event timed out!");
            CloseHandle(serverRunningEvent);
            return false;
        }
        serverThread.detach();
    }
    else
    {
        HANDLE clientRunningEvent = CreateEvent(NULL, TRUE, FALSE, L"ClientRunning");
        if (clientRunningEvent == NULL)
        {
            UtilitiesRuntime::setLastError("Could not create Client Running event");
            return false;
        }

        std::thread clientThread(&ClientServerRuntime::initClient, this);
        if (WaitForSingleObject(clientRunningEvent, 5000) == WAIT_TIMEOUT)
        {
            UtilitiesRuntime::setLastError("Client running event timed out!");
            CloseHandle(clientRunningEvent);
            return false;
        }
        clientThread.detach();
    }
    return true;
}

MonitorState ClientServerRuntime::getOtherMonitorState() const
{
    return static_cast<MonitorState>(ClientServerObject_->getOtherState());
}

void ClientServerRuntime::setCurrentMonitorState(MonitorState state)
{
    State_ = state;
    sendMessageToClients(state);
}

bool ClientServerRuntime::ensureServerEnvironment()
{
    if (!LocalIP_.empty()) return true;
    char IPAddress[30];
    DWORD ret = GetEnvironmentVariableA("RunServer", IPAddress, 30);
    if (ret == 0)
        return false;

    LocalIP_ = IPAddress;
    return true;
}

bool ClientServerRuntime::initServer()
{
    // Check if env variable is defined
    try
    {
        boost::asio::io_service io_service;
        tcp::endpoint endpoint(tcp::v4(), std::atoi(SERVICE_PORT));
        endpoint.address(boost::asio::ip::make_address(LocalIP_));
        ClientServerObject_ = new Server(io_service, endpoint);
        HANDLE serverRunningEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"ServerRunning");

        if (serverRunningEvent != NULL) SetEvent(serverRunningEvent);

        else SET_ERROR_EXIT("Something went wrong trying to signal the server running event!", false);

        CloseHandle(serverRunningEvent);
        io_service.run();
    }
    catch (std::exception& e)
        SET_ERROR_EXIT(string("Encountered an error while starting the server! Error: ") + e.what(), false);

    return 0;

}

bool ClientServerRuntime::initClient()
{
    try
    {
        HANDLE clientRunningEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"ClientRunning");
        boost::asio::io_service io_service;
        tcp::resolver resolver(io_service);
        auto endpoint_iterator = resolver.resolve({ SERVER_IP_ADDRESS, SERVICE_PORT });
        ClientServerObject_ = new Client(io_service, endpoint_iterator, boost::asio::ip::host_name().c_str());

        if (clientRunningEvent != NULL) SetEvent(clientRunningEvent);

        else SET_ERROR_EXIT("Something went wrong trying to signal the client running event!", 0);

        CloseHandle(clientRunningEvent);
        io_service.run();
    }
    catch (std::exception& e)
        SET_ERROR_EXIT(string("Error Connecting client: ") + e.what(), 0);

    return true;
}

void ClientServerRuntime::sendMessageToClients(MonitorState& state)
{
    Message msg(&state);
    ClientServerObject_->getOtherState();
    return ClientServerObject_->write(msg);
}
