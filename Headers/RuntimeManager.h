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

private:
    static std::vector<std::unique_ptr<Runtime>> RuntimeVector_;
};

