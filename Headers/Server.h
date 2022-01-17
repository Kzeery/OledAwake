#pragma once
#include <deque>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <boost/asio.hpp>
#include "Message.h"
#define MAX_RECENT_MESSAGES 100
using boost::asio::ip::tcp;

//----------------------------------------------------------------------

typedef std::deque<Message> Message_queue;

//----------------------------------------------------------------------

class ChatParticipant
{
public:
    virtual ~ChatParticipant() {}
    virtual void deliver(const Message& msg) = 0;
};

typedef std::shared_ptr<ChatParticipant> ChatParticipant_ptr;

//----------------------------------------------------------------------

class Room 
{
public:
    Room() : Name_(boost::asio::ip::host_name()) {};

    void join(ChatParticipant_ptr participant);
    void leave(ChatParticipant_ptr participant);

    void deliver(const Message& msg);
    void write(Message& msg);

    const int getOtherState() const;

private:
    std::set<ChatParticipant_ptr> Participants_;
    Message_queue recent_msgs_;
    Message OtherMessage_;
    std::string Name_;
};

//----------------------------------------------------------------------

class Session
    : public ChatParticipant,
    public std::enable_shared_from_this<Session>
{
public:
    Session(tcp::socket socket, Room& room)
        : Socket_(std::move(socket)),
        Room_(room)
    {
    }

    void start();
    void deliver(const Message& msg);

private:
    void doReadIdentifier();
    void doReadBody();

    void doWrite();

private:
    tcp::socket Socket_;
    Room& Room_;
    Message ReadMessage_;
    Message_queue WriteMessages_;
};

//----------------------------------------------------------------------

class Server : public ReadableClientServerObject
{
public:
    Server(boost::asio::io_service& io_service,
        const tcp::endpoint& endpoint);
    const int getOtherState() const { return Room_.getOtherState(); }
    void write(Message& msg) { 
        return Room_.write(msg); 
    }
private:
    void doAccept();

private:
    tcp::acceptor Acceptor_;
    tcp::socket Socket_;
    Room Room_;
};