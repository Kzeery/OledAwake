#pragma once
#include "ClientServerRuntime.h"
#include <vector>
typedef Runtime* (*GetRuntimeInstanceType)(void);
enum RuntimeIndices
{
    TVCommunicationRuntimeIndex = 0,
    ClientServerRuntimeIndex = 1
};
class RuntimeManager
{
public:
    static bool initAllRuntimes();

    static TVCommunicationRuntime* getTVCommunicationRuntime();
    static ClientServerRuntime* getClientServerRuntime();

private:
    static std::vector<GetRuntimeInstanceType> RuntimeVector_;
    static bool Initialized_;
};

