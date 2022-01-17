#include "Server.h"
void Room::join(ChatParticipant_ptr participant)
{
    Participants_.insert(participant);
    for (const auto& msg : recent_msgs_)
        participant->deliver(msg);
}

void Room::leave(ChatParticipant_ptr participant)
{
    Participants_.erase(participant);
}

void Room::deliver(const Message& msg)
{
    if (msg.getIdentifier() != Name_)
        OtherMessage_ = msg;

    recent_msgs_.push_back(msg);
    while (recent_msgs_.size() > MAX_RECENT_MESSAGES)
        recent_msgs_.pop_front();

    for (auto participant : Participants_)
        participant->deliver(msg);
}

void Room::write(Message& msg)
{
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
        boost::asio::buffer(ReadMessage_.getData(), IDENTIFIER_LENGTH),
        [this, self](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec && ReadMessage_.decodeIdentifier())
            {
                doReadBody();
            }
            else
            {
                Room_.leave(shared_from_this());
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

void Server::doAccept()
{
    Acceptor_.async_accept(Socket_,
        [this](boost::system::error_code ec)
        {
            if (!ec)
            {
                std::make_shared<Session>(std::move(Socket_), Room_)->start();
            }

            doAccept();
        });
}