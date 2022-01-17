#pragma once
#include "ClientServerRuntime.h"
#include <vector>
typedef Runtime* (*GetRuntimeInstanceType)(void);
enum RuntimeIndices
{
    UtilitiesRuntimeIndex = 0,
    ClientServerRuntimeIndex = 1
};
class RuntimeManager
{
public:
    static bool initAllRuntimes();

    static UtilitiesRuntime* getUtilitiesRuntime();
    static ClientServerRuntime* getClientServerRuntime();

private:
    static std::vector<GetRuntimeInstanceType> RuntimeVector_;
    static bool Initialized_;
};

