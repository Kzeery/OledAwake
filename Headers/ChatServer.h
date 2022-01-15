#pragma once
#include <cstdlib>
#include <deque>
#include <iostream>
#include <list>
#include <memory>
#include <set>
#include <utility>
#include <boost/asio.hpp>
#include "ChatMessage.h"

using boost::asio::ip::tcp;

//----------------------------------------------------------------------

typedef std::deque<ChatMessage> ChatMessage_queue;

//----------------------------------------------------------------------

class ChatParticipant
{
public:
    virtual ~ChatParticipant() {}
    virtual void deliver(const ChatMessage& msg) = 0;
};

typedef std::shared_ptr<ChatParticipant> ChatParticipant_ptr;

//----------------------------------------------------------------------

class ChatRoom
{
public:
    void join(ChatParticipant_ptr participant)
    {
        participants_.insert(participant);
        for (const auto& msg : recent_msgs_)
            participant->deliver(msg);
    }

    void leave(ChatParticipant_ptr participant)
    {
        participants_.erase(participant);
    }

    void deliver(const ChatMessage& msg)
    {
        recent_msgs_.push_back(msg);
        while (recent_msgs_.size() > max_recent_msgs)
            recent_msgs_.pop_front();

        for (auto participant : participants_)
            participant->deliver(msg);
    }

private:
    std::set<ChatParticipant_ptr> participants_;
    enum { max_recent_msgs = 100 };
    ChatMessage_queue recent_msgs_;
};

//----------------------------------------------------------------------

class ChatSession
    : public ChatParticipant,
    public std::enable_shared_from_this<ChatSession>
{
public:
    ChatSession(tcp::socket socket, ChatRoom& room)
        : socket_(std::move(socket)),
        room_(room)
    {
    }

    void start()
    {
        room_.join(shared_from_this());
        do_read_header();
    }

    void deliver(const ChatMessage& msg)
    {
        bool write_in_progress = !write_msgs_.empty();
        write_msgs_.push_back(msg);
        if (!write_in_progress)
        {
            do_write();
        }
    }

private:
    void do_read_header()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
            {
                if (!ec && read_msg_.decode_header())
                {
                    do_read_identifier();
                }
                else
                {
                    room_.leave(shared_from_this());
                }
            });
    }
    void do_read_identifier()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data() + ChatMessage::header_length, ChatMessage::identifier_length),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
            {
                if (!ec && read_msg_.decode_identifier())
                {
                    do_read_body();
                }
                else
                {
                    room_.leave(shared_from_this());
                }
            });
    }
    void do_read_body()
    {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    room_.deliver(read_msg_);
                    do_read_header();
                }
                else
                {
                    room_.leave(shared_from_this());
                }
            });
    }

    void do_write()
    {
        auto self(shared_from_this());
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_msgs_.front().data(),
                write_msgs_.front().length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    write_msgs_.pop_front();
                    if (!write_msgs_.empty())
                    {
                        do_write();
                    }
                }
                else
                {
                    room_.leave(shared_from_this());
                }
            });
    }

    tcp::socket socket_;
    ChatRoom& room_;
    ChatMessage read_msg_;
    ChatMessage_queue write_msgs_;
};

//----------------------------------------------------------------------

class ChatServer
{
public:
    ChatServer(boost::asio::io_service& io_service,
        const tcp::endpoint& endpoint)
        : acceptor_(io_service, endpoint),
        socket_(io_service)
    {
        do_accept();
    }

private:
    void do_accept()
    {
        acceptor_.async_accept(socket_,
            [this](boost::system::error_code ec)
            {
                if (!ec)
                {
                    std::make_shared<ChatSession>(std::move(socket_), room_)->start();
                }

                do_accept();
            });
    }

    tcp::acceptor acceptor_;
    tcp::socket socket_;
    ChatRoom room_;
};


//----------------------------------------------------------------------

//int StartServer(char* port)
//{
//    try
//    {
//
//        boost::asio::io_service io_service;
//        tcp::endpoint endpoint(tcp::v4(), std::atoi(port));
//        ChatServer server(io_service, endpoint);
//        io_service.run();
//    }
//    catch (std::exception& e)
//    {
//        std::cerr << "Exception: " << e.what() << "\n";
//    }
//
//    return 0;
//}