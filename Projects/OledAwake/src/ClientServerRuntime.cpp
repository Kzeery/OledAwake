#include "ClientServerRuntime.h"
std::unique_ptr<Runtime> ClientServerRuntime::Instance_ = nullptr;

ClientServerRuntime::~ClientServerRuntime()
{
    if (ClientServerObjectThread_ != nullptr && ClientServerObjectThread_->joinable())
    {
        ClientServerObject_->close();
        ClientServerObject_.reset(nullptr);
        ClientServerObjectThread_->join();
    }
}

Runtime* ClientServerRuntime::getInstance()
{
    if (Instance_.get() == nullptr)
    {
        Instance_ = std::unique_ptr<Runtime>(new ClientServerRuntime);
    }
    return Instance_.get();
}

bool ClientServerRuntime::init()
{
    if (ensureServerEnvironment())
    {
        HANDLE serverRunningEvent = CreateEvent(NULL, TRUE, FALSE, L"ServerRunning");
        if (serverRunningEvent == NULL)
        {
            Utilities::setLastError("Could not create Server Running event");
            return false;
        }

        ClientServerObjectThread_ = std::unique_ptr<std::thread>(new std::thread(&ClientServerRuntime::initServer, this));
        if (WaitForSingleObject(serverRunningEvent, 5000) == WAIT_TIMEOUT)
        {
            ClientServerObjectThread_->join();
            ClientServerObjectThread_.reset(nullptr);
            Utilities::setLastError("Server running event timed out!");
            CloseHandle(serverRunningEvent);
            return false;
        }
        CloseHandle(serverRunningEvent);
    }
    else
    {
        HANDLE clientRunningEvent = CreateEvent(NULL, TRUE, FALSE, L"ClientRunning");
        if (clientRunningEvent == NULL)
        {
            Utilities::setLastError("Could not create Client Running event");
            return false;
        }

        ClientServerObjectThread_ = std::unique_ptr<std::thread>(new std::thread(&ClientServerRuntime::initClient, this));
        if (WaitForSingleObject(clientRunningEvent, 5000) == WAIT_TIMEOUT)
        {
            ClientServerObjectThread_->join();
            ClientServerObjectThread_.reset(nullptr);
            Utilities::setLastError("Client running event timed out!");
            CloseHandle(clientRunningEvent);
            return false;
        }
        CloseHandle(clientRunningEvent);
    }
    return true;
}

MonitorState ClientServerRuntime::getOtherMonitorState() const
{
    return static_cast<MonitorState>(ClientServerObject_->getOtherState());
}
MonitorState ClientServerRuntime::getCurrentMonitorState()
{
    return State_;
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
        ClientServerObject_ = std::unique_ptr<ReadableClientServerObject>(new Server(io_service, endpoint));
        HANDLE serverRunningEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, L"ServerRunning");

        if (serverRunningEvent != NULL) SetEvent(serverRunningEvent);

        else SET_ERROR_EXIT("Something went wrong trying to signal the server running event!", false);

        CloseHandle(serverRunningEvent);
        io_service.run();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (ClientServerObject_ != nullptr)
        {
            HANDLE serverExitedEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, Utilities::eventNames[CLIENT_SERVER_EXITED_EVENT_INDEX]);
            if (!serverExitedEvent) throw std::exception("CRITICAL: COULD NOT OPEN SERVER EXITED EVENT");
            SetEvent(serverExitedEvent);
            CloseHandle(serverExitedEvent);
            ClientServerObject_.reset(nullptr);

        }
    }
    catch (std::exception& e)
        SET_ERROR_EXIT(std::string("Encountered an error while starting the server! Error: ") + e.what(), false);

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
        ClientServerObject_ = std::unique_ptr<ReadableClientServerObject>(new Client(io_service, endpoint_iterator, boost::asio::ip::host_name().c_str()));

        if (clientRunningEvent != NULL) SetEvent(clientRunningEvent);

        else SET_ERROR_EXIT("Something went wrong trying to signal the client running event!", 0);

        CloseHandle(clientRunningEvent);
        io_service.run();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        if (ClientServerObject_ != nullptr)
        {
            HANDLE clientExitedEvent = OpenEvent(EVENT_ALL_ACCESS, FALSE, Utilities::eventNames[CLIENT_SERVER_EXITED_EVENT_INDEX]);
            if (!clientExitedEvent) throw std::exception("CRITICAL: COULD NOT OPEN CLIENT EXITED EVENT");
            SetEvent(clientExitedEvent);
            CloseHandle(clientExitedEvent);
            ClientServerObject_.reset(nullptr);
        }
        
    }
    catch (std::exception& e)
        SET_ERROR_EXIT(std::string("Error Connecting client: ") + e.what(), 0);

    return true;
}

void ClientServerRuntime::sendMessageToClients(MonitorState& state)
{
    int intState = static_cast<int>(state);
    Message msg(&intState);
    return ClientServerObject_->write(msg);
}

bool ClientServerRuntime::cleanUpAndReconnect()
{
    if (ClientServerObjectThread_ != nullptr && ClientServerObjectThread_->joinable())
    {
        ClientServerObjectThread_->join();
        ClientServerObjectThread_.reset(nullptr);
    }
    return init();
}