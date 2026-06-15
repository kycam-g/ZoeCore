/*
 * This file is part of the AzerothCore Project.
 * ZoeCore V1 PT-BR Multi Alert.
 */

#include "Config.h"
#include "Log.h"
#include "ScriptMgr.h"
#include "ServerAutoShutdown.h"
#include "TaskScheduler.h"

class ServerAutoShutdown_World : public WorldScript
{
public:
    ServerAutoShutdown_World() : WorldScript("ServerAutoShutdown_World", {
        WORLDHOOK_ON_UPDATE,
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnUpdate(uint32 diff) override
    {
        sSAS->OnUpdate(diff);
    }

    void OnAfterConfigLoad(bool reload) override
    {
        if (reload)
            sSAS->Init();
    }

    void OnStartup() override
    {
        sSAS->Init();
    }
};

void AddSC_ServerAutoShutdown()
{
    new ServerAutoShutdown_World();
}
