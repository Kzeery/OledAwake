#include "Client.h"
Client::Client(boost::asio::io_service& io_service,
    tcp::resolver::iterator endpoint_iterator,
    const char* name) : IO_Service_(io_service),
    Socket_(io_service),
    Name_(name)
{
    doConnect(endpoint_iterator);
}

void Client::write(Message& msg)
{
    IO_Service_.post(
        [this, msg]()
        {
            bool write_in_progress = !WriteMessages_.empty();
            WriteMessages_.push_back(msg);
            if (!write_in_progress)
            {
                doWrite();
            }
        });
}

const int Client::getOtherState() const
{
    return *(int*) OtherMessage_.getBody();
}

Client::~Client()
{
    IO_Service_.post([this]() { Socket_.close(); });
}


void Client::doConnect(tcp::resolver::iterator endpoint_iterator)
{
    boost::asio::async_connect(Socket_, endpoint_iterator,
        [this](boost::system::error_code ec, tcp::resolver::iterator)
        {
            if (!ec)
            {
                doReadIdentifier();
            }
        });
}

void Client::doReadIdentifier()
{
    boost::asio::async_read(Socket_,
        boost::asio::buffer(ReadMessage_.getData(), IDENTIFIER_LENGTH),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec && ReadMessage_.decodeIdentifier())
            {
                doReadBody();
            }
            else
            {
                Socket_.close();
            }
        });
}

void Client::doReadBody()
{
    boost::asio::async_read(Socket_,
        boost::asio::buffer(ReadMessage_.getBody(), BODY_LENGTH),
        [this](boost::system::error_code ec, std::size_t /*length*/)
        {
            if (!ec)
            {
                if (ReadMessage_.getIdentifier() != Name_)
                    OtherMessage_ = ReadMessage_;
                doReadIdentifier();
            }
            else
            {
                Socket_.close();
            }
        });
}

void Client::doWrite()
{
    boost::asio::async_write(Socket_,
        boost::asio::buffer(WriteMessages_.front().getData(),
            MAX_MESSAGE_LENGTH),
        [this](boost::system::error_code ec, std::size_t /*length*/)
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
                Socket_.close();
            }
        });
}