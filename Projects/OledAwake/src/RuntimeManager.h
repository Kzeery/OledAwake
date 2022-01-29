#pragma once
#include "ClientServerRuntime.h"
#include <vector>
typedef Runtime* (*GetRuntimeInstanceType)(bool);
enum RuntimeIndices
{
    TVCommunicationRuntimeIndex = 0,
    ClientServerRuntimeIndex = 1
};
class RuntimeManager
{
public:
    static bool initAllRuntimes();
    static void destroy();
    static TVCommunicationRuntime* getTVCommunicationRuntime();
    static ClientServerRuntime* getClientServerRuntime();

private:
    static std::vector<GetRuntimeInstanceType> RuntimeVector_;
    static bool Initialized_;
};

