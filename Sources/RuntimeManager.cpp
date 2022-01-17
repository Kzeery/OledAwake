#include "RuntimeManager.h"
vector<Runtime*> RuntimeManager::RuntimeVector_ = {};

bool RuntimeManager::initAllRuntimes()
{
    if (!RuntimeVector_.empty()) return true;
    RuntimeVector_ = { new UtilitiesRuntime() , new ClientServerRuntime() };
    for (const auto& runtime : RuntimeVector_)
    {
        if (!runtime->init())
            return false;
    }
    return true;
}

UtilitiesRuntime* RuntimeManager::getUtilitiesRuntime()
{
    return static_cast<UtilitiesRuntime*>(RuntimeVector_[RuntimeIndices::UtilitiesRuntimeIndex]);
}

ClientServerRuntime* RuntimeManager::getClientServerRuntime()
{
    return static_cast<ClientServerRuntime*>(RuntimeVector_[RuntimeIndices::ClientServerRuntimeIndex]);
}

void RuntimeManager::destroyRuntimes()
{
    for (const auto& runtime : RuntimeVector_)
    {
        delete runtime;
    }
}
