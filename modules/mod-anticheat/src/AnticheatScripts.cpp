/*
 *MIT License
 *
 *Copyright (c) 2023 Azerothcore
 *
 *Permission is hereby granted, free of charge, to any person obtaining a copy
 *of this software and associated documentation files (the "Software"), to deal
 *in the Software without restriction, including without limitation the rights
 *to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *copies of the Software, and to permit persons to whom the Software is
 *furnished to do so, subject to the following conditions:
 *
 *The above copyright notice and this permission notice shall be included in all
 *copies or substantial portions of the Software.
 *
 *THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *SOFTWARE.
 */

#include "Config.h"
#include "AnticheatMgr.h"
#include "Object.h"
#include "AccountMgr.h"
#include "Chat.h"
#include "Player.h"
#include "Timer.h"
#include "GameTime.h"
#include "WorldSessionMgr.h"

#include <cstdlib>
#include <sstream>
#include <string>


namespace
{
    bool ZoeListContains(std::string const& list, uint32 value)
    {
        if (list.empty())
            return false;

        std::stringstream ss(list);
        std::string token;

        while (ss >> token)
        {
            char* end = nullptr;
            unsigned long parsed = std::strtoul(token.c_str(), &end, 10);

            if (end && *end == '\0' && parsed == value)
                return true;
        }

        return false;
    }

    bool ZoeShouldIgnoreMovement(Player* player)
    {
        if (!player)
            return true;

        if (sConfigMgr->GetOption<bool>("Anticheat.Zoe.PvPOnly.Enable", false) && !player->InBattleground() && !player->InArena())
            return true;

        uint32 mapId = player->GetMapId();
        uint32 zoneId = player->GetZoneId();
        uint32 areaId = player->GetAreaId();

        if (ZoeListContains(sConfigMgr->GetOption<std::string>("Anticheat.Zoe.Whitelist.MapIds", ""), mapId))
        {
            if (sConfigMgr->GetOption<bool>("Anticheat.Zoe.Whitelist.Log", false))
                LOG_INFO("module", "Anticheat ZoeCore: movimento ignorado por whitelist de mapa. Player: {} MapId: {}", player->GetName(), mapId);

            return true;
        }

        if (ZoeListContains(sConfigMgr->GetOption<std::string>("Anticheat.Zoe.Whitelist.ZoneIds", ""), zoneId))
        {
            if (sConfigMgr->GetOption<bool>("Anticheat.Zoe.Whitelist.Log", false))
                LOG_INFO("module", "Anticheat ZoeCore: movimento ignorado por whitelist de zona. Player: {} ZoneId: {}", player->GetName(), zoneId);

            return true;
        }

        if (ZoeListContains(sConfigMgr->GetOption<std::string>("Anticheat.Zoe.Whitelist.AreaIds", ""), areaId))
        {
            if (sConfigMgr->GetOption<bool>("Anticheat.Zoe.Whitelist.Log", false))
                LOG_INFO("module", "Anticheat ZoeCore: movimento ignorado por whitelist de area. Player: {} AreaId: {}", player->GetName(), areaId);

            return true;
        }

        return false;
    }
}

Seconds resetTime = 0s;
Seconds lastIterationPlayer = GameTime::GetUptime() + 30s; //TODO: change 30 secs static to a configurable option

class AnticheatPlayerScript : public PlayerScript
{
public:
    AnticheatPlayerScript() : PlayerScript("AnticheatPlayerScript", { PLAYERHOOK_ON_LOGOUT, PLAYERHOOK_ON_LOGIN, PLAYERHOOK_ON_UPDATE }) { }

    void OnPlayerLogout(Player* player) override
    {
        sAnticheatMgr->HandlePlayerLogout(player);
    }

    void OnPlayerLogin(Player* player) override
    {
        sAnticheatMgr->HandlePlayerLogin(player);

        if (sConfigMgr->GetOption<bool>("Anticheat.LoginMessage", true))
            ChatHandler(player->GetSession()).PSendSysMessage("Este servidor usa o sistema Anticheat ZoeCore.");
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (sConfigMgr->GetOption<bool>("Anticheat.OpAckOrderHack", true) && sConfigMgr->GetOption<bool>("Anticheat.Enabled", true))
            sAnticheatMgr->AckUpdate(player, diff);
    }
};

class AnticheatWorldScript : public WorldScript
{
public:
    AnticheatWorldScript() : WorldScript("AnticheatWorldScript", { WORLDHOOK_ON_UPDATE, WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

    void OnUpdate(uint32 /* diff */) override // unusued parameter
    {
        if (GameTime::GetGameTime() > resetTime)
        {
            LOG_INFO("module", "Anticheat: Resetando estados de report diario.");
            sAnticheatMgr->ResetDailyReportStates();
            UpdateReportResetTime();
            LOG_INFO("module", "Anticheat: Proximo reset diario de reports: {}", Acore::Time::TimeToHumanReadable(resetTime));
        }

        if (GameTime::GetUptime() > lastIterationPlayer)
        {
            lastIterationPlayer = GameTime::GetUptime() + Seconds(sConfigMgr->GetOption<uint32>("Anticheat.SaveReportsTime", 60));

            LOG_INFO("module", "Anticheat: Salvando reports de {} jogadores.", sWorldSessionMgr->GetPlayerCount());

            WorldSessionMgr::SessionMap const& sessionMap = sWorldSessionMgr->GetAllSessions();
            for (WorldSessionMgr::SessionMap::const_iterator itr = sessionMap.begin(); itr != sessionMap.end(); ++itr)
                if (Player* plr = itr->second->GetPlayer())
                    sAnticheatMgr->SavePlayerData(plr);
        }
    }

    void OnAfterConfigLoad(bool /* reload */) override // unusued parameter
    {
        LOG_INFO("module", "AnticheatModule ZoeCore carregado.");
    }

    void UpdateReportResetTime()
    {
        resetTime = Seconds(Acore::Time::GetNextTimeWithDayAndHour(-1, 6));
    }
};

class AnticheatMovementHandlerScript : public MovementHandlerScript
{
public:
    AnticheatMovementHandlerScript() : MovementHandlerScript("AnticheatMovementHandlerScript", { MOVEMENTHOOK_ON_PLAYER_MOVE }) { }

    void OnPlayerMove(Player* player, MovementInfo mi, uint32 opcode) override
    {
        if (!player || !player->GetSession())
            return;

        if (ZoeShouldIgnoreMovement(player))
            return;

        if (!player->GetSession()->IsGMAccount() || sConfigMgr->GetOption<bool>("Anticheat.EnabledOnGmAccounts", false))
            sAnticheatMgr->StartHackDetection(player, mi, opcode);
    }
};

void startAnticheatScripts()
{
    new AnticheatWorldScript();
    new AnticheatPlayerScript();
    new AnticheatMovementHandlerScript();
}
