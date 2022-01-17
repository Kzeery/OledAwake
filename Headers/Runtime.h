#pragma once
class Runtime
{
public:
    virtual ~Runtime() { };
    virtual bool init() = 0;
    //virtual Runtime* getInstance() = 0;
};

