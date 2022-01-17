#pragma once
class Runtime
{
public:
    virtual ~Runtime() {};
    virtual bool init() = 0;
};