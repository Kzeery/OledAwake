#include "RuntimeManager.h"
std::vector<GetRuntimeInstanceType> RuntimeManager::RuntimeVector_ = {};
bool RuntimeManager::Initialized_ = false;
bool RuntimeManager::initAllRuntimes()
{
    if (Initialized_) return true;

    RuntimeVector_.push_back(TVCommunicationRuntime::getInstance);
    RuntimeVector_.push_back(ClientServerRuntime::getInstance);

    for (const GetRuntimeInstanceType& getInstance : RuntimeVector_)
    {
        Runtime* instance = getInstance(true); // attempt to get a new instance
        if (!instance || !instance->init())
        {
            destroy();
            return false;
        }
    }
        
    Initialized_ = true;
    return true;
}
void RuntimeManager::destroy()
{
    for (const GetRuntimeInstanceType& getInstance : RuntimeVector_)
    {
        Runtime* instance = getInstance(false); // only looking to destroy instances that already exist. Dont make new
        if (instance) instance->destroy().reset(nullptr);
    }
}

TVCommunicationRuntime* RuntimeManager::getTVCommunicationRuntime()
{
    if (!Initialized_) throw std::exception("Attempting to get uninitialized memory!");
    TVCommunicationRuntime* pRet = static_cast<TVCommunicationRuntime*>(RuntimeVector_[RuntimeIndices::TVCommunicationRuntimeIndex](false));
    if(pRet == nullptr) throw std::exception("TV Communication is NULL!");
    return pRet;
}

ClientServerRuntime* RuntimeManager::getClientServerRuntime()
{
    if (!Initialized_) throw std::exception("Attempting to get uninitialized memory!");
    ClientServerRuntime* pRet = static_cast<ClientServerRuntime*>(RuntimeVector_[RuntimeIndices::ClientServerRuntimeIndex](false));
    if (pRet == nullptr) throw std::exception("ClientServer Runtime is NULL!");
    return pRet;
}

