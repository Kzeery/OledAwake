#include "RuntimeManager.h"
vector<unique_ptr<Runtime>> RuntimeManager::RuntimeVector_ = {};

bool RuntimeManager::initAllRuntimes()
{
    if (!RuntimeVector_.empty()) return true;
    auto UtilitiesRuntimePtr = unique_ptr<Runtime>(new UtilitiesRuntime);
    auto ClientServerRuntimePtr = unique_ptr<Runtime>(new ClientServerRuntime);

    RuntimeVector_.push_back(move(UtilitiesRuntimePtr));
    RuntimeVector_.push_back(move(ClientServerRuntimePtr));

    for (const auto& runtime : RuntimeVector_)
    {
        if (!runtime->init())
            return false;
    }
    return true;
}

UtilitiesRuntime* RuntimeManager::getUtilitiesRuntime()
{
    return static_cast<UtilitiesRuntime*>(RuntimeVector_[RuntimeIndices::UtilitiesRuntimeIndex].get());
}

ClientServerRuntime* RuntimeManager::getClientServerRuntime()
{
    return static_cast<ClientServerRuntime*>(RuntimeVector_[RuntimeIndices::ClientServerRuntimeIndex].get());
}

