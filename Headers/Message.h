#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <boost/asio.hpp>
#include <vector>
#define IDENTIFIER_LENGTH 32
#define BODY_LENGTH sizeof(int)
#define MAX_MESSAGE_LENGTH  IDENTIFIER_LENGTH + BODY_LENGTH
#define BODY_PTR Data_.data() + IDENTIFIER_LENGTH

class Message;

class ReadableClientServerObject
{
public:
    virtual const int getOtherState() const = 0;
    virtual void write(Message& msg) = 0;
    virtual void close() = 0;
    virtual ~ReadableClientServerObject() {};
};
class Message
{
public:
    Message() : Data_(std::vector<char>(MAX_MESSAGE_LENGTH, 0)) 
    {
        setIdentifier(boost::asio::ip::host_name());
    }

    Message(void* messageBody) :  Data_(std::vector<char>(MAX_MESSAGE_LENGTH, 0))
    {
        std::memcpy(BODY_PTR, messageBody, BODY_LENGTH);
        setIdentifier(boost::asio::ip::host_name());
    }

    const char* getData() const
    {
        return Data_.data();
    }

    char* getData()
    {
        return Data_.data();
    }

    const char* getBody() const
    {
        return BODY_PTR;
    }

    char* getBody()
    {
        return BODY_PTR;
    }

    const std::string getIdentifier() const
    {
        return Identifier_;
    }

    bool decodeIdentifier()
    {
        char identifier[IDENTIFIER_LENGTH + 1] = "";
        std::memcpy(identifier, Data_.data(), IDENTIFIER_LENGTH);
        Identifier_ = identifier;
        return true;
    }
    
private:
    void setIdentifier(std::string identifier)
    {
        if (identifier.size() > IDENTIFIER_LENGTH)
            identifier = identifier.substr(0, IDENTIFIER_LENGTH);
        Identifier_ = std::move(identifier);
        encodeIdentifier();
    }

    void encodeIdentifier()
    {
        std::memcpy(Data_.data(), Identifier_.c_str(), Identifier_.size());
    }
    std::vector<char> Data_;
    std::string Identifier_;
};