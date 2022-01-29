#pragma once
class Runtime
{
public:
    virtual ~Runtime() { };
private:
    virtual bool init() = 0;
    virtual std::unique_ptr<Runtime>& destroy() = 0;
    friend class RuntimeManager;
};

