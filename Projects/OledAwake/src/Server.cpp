#include "Server.h"
#include "Utilities.h"
#include "RuntimeManager.h"
void Room::join(ChatParticipant_ptr participant)
{
    Participants_.insert(participant);
    int state = static_cast<int>(RuntimeManager::getClientServerRuntime()->getCurrentMonitorState());
    Message msg(&state);
    msg.setIdentifier(Name_);
    participant->deliver(msg);
    
}

void Room::leave(ChatParticipant_ptr participant)
{
    Participants_.erase(participant);
    if (Participants_.size() == 0)
    {
        memset(OtherMessage_.getBody(), 0, BODY_LENGTH);
    }
}

void Room::deliver(const Message& msg)
{
    if (msg.getIdentifier() != Name_)
        memcpy(OtherMessage_.getData(), msg.getData(), MAX_MESSAGE_LENGTH);

    for (auto participant : Participants_)
        participant->deliver(msg);
}

void Room::write(Message& msg)
{
    msg.setIdentifier(Name_);
    deliver(msg);
}

const int Room::getOtherState() const
{
    return *(int*)OtherMessage_.getBody();
}

void Session::start()
{
    Room_.join(shared_from_this());
    doReadIdentifier();
}

void Session::deliver(const Message& msg)
{
    bool write_in_progress = !WriteMessages_.empty();
    WriteMessages_.push_back(msg);
    if (!write_in_progress)
    {
        doWrite();
    }
}

void Session::doReadIdentifier()
{
    auto self(shared_from_this());
    boost::asio::async_read(Socket_,
        boost::asio::buffer(ReadMessage_.getIdentifier(), IDENTIFIER_LENGTH),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
                doReadBody();
            else
            {
                Room_.leave(shared_from_this());
                Utilities::setLastError(std::string("Session doReadIdentifier encountered an error: ") + ec.message());
            }
        });
}
void Session::doReadBody()
{
    auto self(shared_from_this());
    boost::asio::async_read(Socket_,
        boost::asio::buffer(ReadMessage_.getBody(), BODY_LENGTH),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                Room_.deliver(ReadMessage_);
                doReadIdentifier();
            }
            else
            {
                Room_.leave(shared_from_this());
                Utilities::setLastError(std::string("Session doReadBody encountered an error: ") + ec.message());
            }
        });
}

void Session::doWrite()
{
    auto self(shared_from_this());
    boost::asio::async_write(Socket_,
        boost::asio::buffer(WriteMessages_.front().getData(),
            MAX_MESSAGE_LENGTH),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                WriteMessages_.pop_front();
                if (!WriteMessages_.empty())
                {
                    doWrite();
                }
            }
            else
            {
                Room_.leave(shared_from_this());
                Utilities::setLastError(std::string("Session doWrite encountered an error: ") + ec.message());

            }
        });
}

Server::Server(boost::asio::io_service& io_service,
    const tcp::endpoint& endpoint)
    : Acceptor_(io_service, endpoint),
    Socket_(io_service),
    Room_(Room())
{
    doAccept();
}

void Server::close()
{
    boost::asio::post(Acceptor_.get_executor(), [this] {Acceptor_.cancel(); });
}

void Server::write(Message& msg) {
    return Room_.write(msg);
}

void Server::doAccept()
{
    Acceptor_.async_accept(Socket_,
        [this](boost::system::error_code ec)
        {
            if (ec) {
                Utilities::setLastError(std::string("Server doAccept encountered an error: ") + ec.message());
                return;
            }

            std::make_shared<Session>(std::move(Socket_), Room_)->start();
            doAccept();
        });
}