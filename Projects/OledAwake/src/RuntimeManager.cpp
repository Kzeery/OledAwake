#include "RuntimeManager.h"
std::vector<GetRuntimeInstanceType> RuntimeManager::RuntimeVector_ = {};
bool RuntimeManager::Initialized_ = false;
bool RuntimeManager::initAllRuntimes()
{
    if (Initialized_) return true;

    RuntimeVector_.push_back(TVCommunicationRuntime::getInstance);
    RuntimeVector_.push_back(ClientServerRuntime::getInstance);

    for (const auto& runtime : RuntimeVector_)
        if (!runtime()->init()) return false;

    Initialized_ = true;
    return true;
}

TVCommunicationRuntime* RuntimeManager::getTVCommunicationRuntime()
{
    if (!initAllRuntimes()) throw std::exception("Cannot get TV Communication Runtime!");
    auto pRet = static_cast<TVCommunicationRuntime*>(RuntimeVector_[RuntimeIndices::TVCommunicationRuntimeIndex]());
    if(pRet == nullptr) throw std::exception("TV Communication is NULL!");
    return pRet;
}

ClientServerRuntime* RuntimeManager::getClientServerRuntime()
{
    if (!initAllRuntimes()) throw std::exception("Cannot get ClientServer Runtime!");
    auto pRet = static_cast<ClientServerRuntime*>(RuntimeVector_[RuntimeIndices::ClientServerRuntimeIndex]());
    if (pRet == nullptr) throw std::exception("ClientServer Runtime is NULL!");
    return pRet;
}

