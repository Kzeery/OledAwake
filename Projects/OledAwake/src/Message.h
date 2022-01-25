#pragma once
#include <SDKDDKVer.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <boost/asio.hpp>
#include <vector>
#define IDENTIFIER_LENGTH 32
#define BODY_LENGTH sizeof(int)
#define MAX_MESSAGE_LENGTH  IDENTIFIER_LENGTH + BODY_LENGTH + 1

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
    Message()
    {
    }

    Message(int* messageBody)
    {
        std::memcpy(MessageData_.Body_, messageBody, BODY_LENGTH);
    }

    char* getIdentifier()
    {
        return MessageData_.Identifier_;
    }

    char* getBody()
    {
        return MessageData_.Body_;
    }
    const char* getBody() const
    {
        return MessageData_.Body_;
    }

    const char* getIdentifier() const
    {
        return MessageData_.Identifier_;
    }

    char* getData()
    {
        return (char*)&MessageData_;
    }
    const char* getData() const
    {
        return (char*)&MessageData_;
    }

    void setIdentifier(const std::string& hostName)
    {
        size_t sz = hostName.size() > IDENTIFIER_LENGTH ? IDENTIFIER_LENGTH : hostName.size();
        std::memcpy(MessageData_.Identifier_, hostName.c_str(), sz);
    }
private:
    struct MessageDataStruct
    {
        char Identifier_[IDENTIFIER_LENGTH]{ 0 };
        char Body_[BODY_LENGTH]{ 0 };
        char RESERVED[1]{ 0 };
    } MessageData_;
};