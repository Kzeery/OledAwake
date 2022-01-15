#pragma once
#include <cstdlib>
#include <deque>
#include <iostream>
#include <thread>
#include <boost/asio.hpp>
#include "ChatMessage.h"

using boost::asio::ip::tcp;

typedef std::deque<ChatMessage> ChatMessage_queue;

class ChatClient
{
public:
    ChatClient(boost::asio::io_service& io_service,
        tcp::resolver::iterator endpoint_iterator,
        const char* name)
        : io_service_(io_service),
        socket_(io_service),
        name_(name)
    {
        do_connect(endpoint_iterator);
    }

    void write(ChatMessage& msg)
    {
        msg.identifier(name_);
        io_service_.post(
            [this, msg]()
            {
                bool write_in_progress = !write_msgs_.empty();
                write_msgs_.push_back(msg);
                if (!write_in_progress)
                {
                    do_write();
                }
            });
    }

    const char* getOtherState()
    {
        return other_msg_.body();
    }

    void close()
    {
        io_service_.post([this]() { socket_.close(); });
    }

private:
    void do_connect(tcp::resolver::iterator endpoint_iterator)
    {
        boost::asio::async_connect(socket_, endpoint_iterator,
            [this](boost::system::error_code ec, tcp::resolver::iterator)
            {
                if (!ec)
                {
                    do_read_header();
                }
            });
    }

    void do_read_header()
    {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data(), ChatMessage::header_length),
            [this](boost::system::error_code ec, std::size_t /*length*/)
            {
                if (!ec && read_msg_.decode_header())
                {
                    do_read_identifier();
                }
                else
                {
                    socket_.close();
                }
            });
    }

    void do_read_identifier()
    {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data() + ChatMessage::header_length, ChatMessage::identifier_length),
            [this](boost::system::error_code ec, std::size_t /*length*/)
            {
                if (!ec && read_msg_.decode_identifier())
                {
                    do_read_body();
                }
                else
                {
                    socket_.close();
                }
            });
    }

    void do_read_body()
    {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length()),
            [this](boost::system::error_code ec, std::size_t /*length*/)
            {
                if (!ec)
                {
                    if (read_msg_.identifier() != name_)
                        other_msg_ = read_msg_;
                    do_read_header();
                }
                else
                {
                    socket_.close();
                }
            });
    }

   

    void do_write()
    {
        boost::asio::async_write(socket_,
            boost::asio::buffer(write_msgs_.front().data(),
                write_msgs_.front().length()),
            [this](boost::system::error_code ec, std::size_t /*length*/)
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
                    socket_.close();
                }
            });
    }

private:
    boost::asio::io_service& io_service_;
    tcp::socket socket_;
    ChatMessage other_msg_;
    ChatMessage read_msg_;
    ChatMessage_queue write_msgs_;
    const std::string name_;
};
