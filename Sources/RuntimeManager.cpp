#include "RuntimeManager.h"
vector<GetRuntimeInstanceType> RuntimeManager::RuntimeVector_ = {};
bool RuntimeManager::Initialized_ = false;
bool RuntimeManager::initAllRuntimes()
{
    if (Initialized_) return true;

    RuntimeVector_.push_back(UtilitiesRuntime::getInstance);
    RuntimeVector_.push_back(ClientServerRuntime::getInstance);

    for (const auto& runtime : RuntimeVector_)
        if (!runtime()->init()) return false;

    Initialized_ = true;
    return true;
}

UtilitiesRuntime* RuntimeManager::getUtilitiesRuntime()
{
    if (!initAllRuntimes()) throw std::exception("Cannot get Utilities Runtime!");
    auto pRet = static_cast<UtilitiesRuntime*>(RuntimeVector_[RuntimeIndices::UtilitiesRuntimeIndex]());
    if(pRet == nullptr) throw std::exception("Utilites Runtime is NULL!");
    return pRet;
}

ClientServerRuntime* RuntimeManager::getClientServerRuntime()
{
    if (!initAllRuntimes()) throw std::exception("Cannot get ClientServer Runtime!");
    auto pRet = static_cast<ClientServerRuntime*>(RuntimeVector_[RuntimeIndices::ClientServerRuntimeIndex]());
    if (pRet == nullptr) throw std::exception("ClientServer Runtime is NULL!");
    return pRet;
}

