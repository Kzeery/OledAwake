#pragma once
#include <cstdio>
#include <cstdlib>
#include <cstring>

class ChatMessage
{
public:
    enum { header_length = 4 };
    enum { identifier_length = 20 };
    enum { max_body_length = 492 };

    ChatMessage()
        : body_length_(0)
    {
        memset(data_, 0, header_length + identifier_length + max_body_length);
    }

    const char* data() const
    {
        return data_;
    }

    char* data()
    {
        return data_;
    }

    std::size_t length() const
    {
        return header_length + identifier_length + body_length_;
    }

    const char* body() const
    {
        return data_ + header_length + identifier_length;
    }

    char* body()
    {
        return data_ + header_length + identifier_length;
    }

    std::size_t body_length() const
    {
        return body_length_;
    }

    void body_length(std::size_t new_length)
    {
        body_length_ = new_length;
        if (body_length_ > max_body_length)
            body_length_ = max_body_length;
    }
    std::string identifier() const
    {
        return identifier_;
    }
    void identifier(std::string identify)
    {
        if (identify.size() > identifier_length)
            identify = identify.substr(0, identifier_length);
        identifier_ = std::move(identify);
        encode_identifier();
    }
    bool decode_header()
    {
        char header[header_length + 1] = "";
        strncat_s(header, header_length + 1, data_, header_length);
        body_length_ = std::atoi(header);
        if (body_length_ > max_body_length)
        {
            body_length_ = 0;
            return false;
        }
        return true;
    }

    void encode_header()
    {
        char header[header_length + 1] = "";
        sprintf_s(header, header_length + 1, "%4d", static_cast<int>(body_length_));
        std::memcpy(data_, header, header_length);
    }

    bool decode_identifier()
    {
        char identifier[identifier_length + 1] = "";
        std::memcpy(identifier, data_ + header_length, identifier_length);
        identifier_ = identifier;
        return true;
    }
    void encode_identifier()
    {
        std::memcpy(data_ + header_length, identifier_.c_str(), identifier_.size());
    }


private:
    char data_[header_length + max_body_length + identifier_length];
    std::size_t body_length_;
    std::string identifier_;
};