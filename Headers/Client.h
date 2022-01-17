#pragma once
#include <cstdlib>
#include <deque>
#include <boost/asio.hpp>
#include "Message.h"

using boost::asio::ip::tcp;

typedef std::deque<Message> Message_queue;

class Client : public ReadableClientServerObject
{
public:
    Client(boost::asio::io_service& io_service,
        tcp::resolver::iterator endpoint_iterator,
        const char* name);
    ~Client();
    void write(Message& msg);
    const int getOtherState() const;

private:
    void doConnect(tcp::resolver::iterator endpoint_iterator);
    void doReadIdentifier();
    void doReadBody();
    void doWrite();

private:
    boost::asio::io_service& IO_Service_;
    tcp::socket Socket_;
    Message OtherMessage_;
    Message ReadMessage_;
    Message_queue WriteMessages_;
    const std::string Name_;
};