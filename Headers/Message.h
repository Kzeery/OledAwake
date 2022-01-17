#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <boost/asio.hpp>
#define IDENTIFIER_LENGTH 32
#define BODY_LENGTH 32
#define MAX_MESSAGE_LENGTH  IDENTIFIER_LENGTH + BODY_LENGTH
class Message;

class ReadableClientServerObject
{
public:
    virtual const int getOtherState() const = 0;
    virtual void write(Message& msg) = 0;
    virtual ~ReadableClientServerObject() {};
};

class Message
{
public:
    Message() :  Data_(new char[MAX_MESSAGE_LENGTH]), Body_(Data_ + IDENTIFIER_LENGTH)
    {
        memset(Data_, 0, MAX_MESSAGE_LENGTH);
    }

    Message(void* messageBody) :  Data_(new char[MAX_MESSAGE_LENGTH]), Body_(Data_ + IDENTIFIER_LENGTH)
    {
        memset(Data_, 0, MAX_MESSAGE_LENGTH);
        std::memcpy(Body_, messageBody, BODY_LENGTH);
        setIdentifier(boost::asio::ip::host_name());
    }

    ~Message()
    {
        delete[] Data_;
    }

    const char* getData() const
    {
        return Data_;
    }

    char* getData()
    {
        return Data_;
    }

    const char* getBody() const
    {
        return Body_;
    }

    char* getBody()
    {
        return Body_;
    }

    const std::string getIdentifier() const
    {
        return Identifier_;
    }

    bool decodeIdentifier()
    {
        char identifier[IDENTIFIER_LENGTH + 1] = "";
        std::memcpy(identifier, Data_, IDENTIFIER_LENGTH);
        Identifier_ = identifier;
        return true;
    }
    
private:
    void setIdentifier(std::string& identifier)
    {
        if (identifier.size() > IDENTIFIER_LENGTH)
            identifier = identifier.substr(0, IDENTIFIER_LENGTH);
        Identifier_ = std::move(identifier);
        encodeIdentifier();
    }

    void encodeIdentifier()
    {
        std::memcpy(Data_, Identifier_.c_str(), Identifier_.size());
    }

    char* Data_;
    char* Body_;
    std::string Identifier_;
};