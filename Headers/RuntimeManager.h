#pragma once
#include "ClientServerRuntime.h"
#include <vector>
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

    static void destroyRuntimes();

private:
    static std::vector<Runtime*> RuntimeVector_;
};

