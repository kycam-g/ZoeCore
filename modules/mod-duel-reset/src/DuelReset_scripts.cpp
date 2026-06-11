/*
 *  Originally written  for TrinityCore by ShinDarth and GigaDev90 (www.trinitycore.org)
 *  Converted as module for AzerothCore by ShinDarth and Yehonal   (www.azerothcore.org)
 *  Reworked by Gozzim
 *  ZoeCore final customization.
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Pet.h"
#include "SpellInfo.h"
#include "DuelReset.h"
#include "Chat.h"
#include "Log.h"
#include "Config.h"
#include "Map.h"
#include "WorldSession.h"
#include <sstream>
#include <vector>

namespace
{
    bool ZoeDuelResetEnabled()
    {
        return sConfigMgr->GetOption<bool>("DuelReset.Zoe.Enable", true);
    }

    std::string ZoeDuelResetPrefix()
    {
        return sConfigMgr->GetOption<std::string>("DuelReset.Zoe.MessagePrefix", "[ZoeCore Duel]");
    }

    bool ZoeDuelResetAnnounce()
    {
        return sConfigMgr->GetOption<bool>("DuelReset.Zoe.Announce", true);
    }

    bool ZoeDuelResetLogEnabled()
    {
        return sConfigMgr->GetOption<bool>("DuelReset.Zoe.Log.Enable", true);
    }

    void ZoeDuelResetMessage(Player* player, std::string const& message)
    {
        if (!ZoeDuelResetEnabled() || !ZoeDuelResetAnnounce() || !player || !player->GetSession())
            return;

        ChatHandler(player->GetSession()).SendSysMessage((ZoeDuelResetPrefix() + " " + message).c_str());
    }

    void ZoeDuelResetLog(std::string const& message)
    {
        if (!ZoeDuelResetEnabled() || !ZoeDuelResetLogEnabled())
            return;

        LOG_INFO("module", "ZoeCore DuelReset: {}", message);
    }

    void ZoeDuelResetReplaceAll(std::string& text, std::string const& search, std::string const& replace)
    {
        if (search.empty())
            return;

        size_t pos = 0;
        while ((pos = text.find(search, pos)) != std::string::npos)
        {
            text.replace(pos, search.length(), replace);
            pos += replace.length();
        }
    }

    bool ZoeDuelResetBlockedInstance(Player* player1, Player* player2)
    {
        if (!sConfigMgr->GetOption<bool>("DuelReset.Zoe.BlockInBattlegroundOrArena", true))
            return false;

        if ((player1 && player1->GetBattleground()) || (player2 && player2->GetBattleground()))
            return true;

        return false;
    }

    std::vector<uint32> ZoeDuelResetParseSpellList(std::string const& value)
    {
        std::vector<uint32> spells;

        std::stringstream ss(value);
        std::string token;

        while (std::getline(ss, token, ';'))
        {
            if (token.empty())
                continue;

            try
            {
                uint32 spellId = uint32(std::stoul(token));
                if (spellId > 0)
                    spells.push_back(spellId);
            }
            catch (...)
            {
            }
        }

        return spells;
    }

    void ZoeDuelResetResetConfiguredCooldowns(Player* player, char const* when)
    {
        if (!player || !sConfigMgr->GetOption<bool>("DuelReset.Zoe.Cooldown.ExtraReset.Enable", true))
            return;

        std::string list = sConfigMgr->GetOption<std::string>("DuelReset.Zoe.Cooldown.ExtraResetSpellIds", "633;2800;10310;27154;48788");
        std::vector<uint32> spells = ZoeDuelResetParseSpellList(list);

        for (uint32 spellId : spells)
            player->RemoveSpellCooldown(spellId, true);

        if (!spells.empty())
            ZoeDuelResetLog("extra cooldown reset player='" + player->GetName() + "' when='" + std::string(when) + "' spells=" + std::to_string(spells.size()));
    }

    void ZoeDuelResetSetPowerSafe(Player* player, Powers power, int32 value)
    {
        if (!player)
            return;

        if (player->GetMaxPower(power) <= 0)
            return;

        player->SetPower(power, value);
    }

    void ZoeDuelResetResetPowers(Player* player)
    {
        if (!player || !sConfigMgr->GetOption<bool>("DuelReset.Zoe.Power.Reset", true))
            return;

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.Power.Mana.Full", true))
            ZoeDuelResetSetPowerSafe(player, POWER_MANA, player->GetMaxPower(POWER_MANA));

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.Power.Rage.Zero", true))
            ZoeDuelResetSetPowerSafe(player, POWER_RAGE, 0);

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.Power.Energy.Full", true))
            ZoeDuelResetSetPowerSafe(player, POWER_ENERGY, player->GetMaxPower(POWER_ENERGY));

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.Power.RunicPower.Zero", true))
            ZoeDuelResetSetPowerSafe(player, POWER_RUNIC_POWER, 0);
    }

    void ZoeDuelResetFullHealthPower(Player* player)
    {
        if (!player)
            return;

        player->SetFullHealth();
        ZoeDuelResetResetPowers(player);
    }

    void ZoeDuelResetRemoveConfiguredDebuffs(Player* player, char const* when)
    {
        if (!player || !sConfigMgr->GetOption<bool>("DuelReset.Zoe.RemoveDebuffs.Enable", true))
            return;

        std::string list = sConfigMgr->GetOption<std::string>("DuelReset.Zoe.RemoveDebuffs.SpellIds", "");

        std::string extraList = sConfigMgr->GetOption<std::string>("DuelReset.Zoe.RemoveDebuffs.ExtraSpellIds", "");
        if (!extraList.empty())
        {
            if (!list.empty())
                list += ";";

            list += extraList;
        }

        std::vector<uint32> spells = ZoeDuelResetParseSpellList(list);
        uint32 passes = sConfigMgr->GetOption<uint32>("DuelReset.Zoe.RemoveDebuffs.Passes", 2);

        if (passes < 1)
            passes = 1;

        if (passes > 5)
            passes = 5;

        uint32 removedAttempts = 0;

        for (uint32 pass = 0; pass < passes; ++pass)
        {
            for (uint32 spellId : spells)
            {
                if (player->HasAura(spellId))
                    ++removedAttempts;

                player->RemoveAurasDueToSpell(spellId);
            }
        }

        if (!spells.empty())
            ZoeDuelResetLog("removed configured debuffs player='" + player->GetName() + "' when='" + std::string(when) + "' spells=" + std::to_string(spells.size()) + " hits=" + std::to_string(removedAttempts));
    }

    void ZoeDuelResetPreparePlayerForDuel(Player* player)
    {
        if (!player)
            return;

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.StartFullHealthMana", true))
            ZoeDuelResetFullHealthPower(player);
        else
            ZoeDuelResetResetPowers(player);

        // Debuff cleanup is executed after the HP/power block so it works even if HealthMana is disabled.
    }

    void ZoeDuelResetCleanAfterDuel(Player* player)
    {
        if (!player)
            return;

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.RemoveDebuffs.OnEnd", true))
            ZoeDuelResetRemoveConfiguredDebuffs(player, "end");

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.Power.ResetAfterDuel", true))
            ZoeDuelResetResetPowers(player);
    }

    void ZoeDuelResetSendTopNotification(Player* player, std::string const& message)
    {
        if (!player || !player->GetSession())
            return;

        // Compatível com esta core: WorldSession não possui SendNotification.
        // Importante: nesta core o SendAreaTriggerMessage não formata "%s".
        // Por isso enviamos a mensagem já pronta diretamente.
        player->GetSession()->SendAreaTriggerMessage(message.c_str());
    }

    void ZoeDuelResetAnnounceWinnerToZone(Player* winner, Player* loser)
    {
        if (!winner || !loser)
            return;

        if (!sConfigMgr->GetOption<bool>("DuelReset.Zoe.WinAnnounce.Enable", true))
            return;

        uint32 zoneId = winner->GetZoneId();
        uint32 areaId = winner->GetAreaId();

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.WinAnnounce.OnlyAllowedArea", true) && !sDuelReset->IsAllowedInArea(winner))
            return;

        std::string message = sConfigMgr->GetOption<std::string>(
            "DuelReset.Zoe.WinAnnounce.Message",
            "|cff00ff00{winner}|r venceu um duelo contra |cffff2020{loser}|r!"
        );

        ZoeDuelResetReplaceAll(message, "{winner}", winner->GetName());
        ZoeDuelResetReplaceAll(message, "{loser}", loser->GetName());

        bool zoneOnly = sConfigMgr->GetOption<bool>("DuelReset.Zoe.WinAnnounce.ZoneOnly", true);
        bool areaOnly = sConfigMgr->GetOption<bool>("DuelReset.Zoe.WinAnnounce.AreaOnly", false);

        Map* map = winner->GetMap();
        if (!map)
            return;

        Map::PlayerList const& players = map->GetPlayers();

        uint32 receivers = 0;

        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            Player* receiver = itr->GetSource();
            if (!receiver || !receiver->IsInWorld())
                continue;

            if (zoneOnly && receiver->GetZoneId() != zoneId)
                continue;

            if (areaOnly && receiver->GetAreaId() != areaId)
                continue;

            ZoeDuelResetSendTopNotification(receiver, message);
            ++receivers;
        }

        ZoeDuelResetLog("zone winner announce winner='" + winner->GetName() + "' loser='" + loser->GetName() + "' zone=" + std::to_string(zoneId) + " receivers=" + std::to_string(receivers));
    }

    void ZoeDuelResetClearPlayers(Player* player1, Player* player2)
    {
        sDuelReset->ClearSavedState(player1);
        sDuelReset->ClearSavedState(player2);
    }
}

class DuelResetAfterConfigLoad : public WorldScript
{
public:
    DuelResetAfterConfigLoad() : WorldScript("DuelResetAfterConfigLoad", {
        WORLDHOOK_ON_AFTER_CONFIG_LOAD,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnAfterConfigLoad(bool reload) override
    {
        sDuelReset->LoadConfig(reload);

        if (ZoeDuelResetEnabled())
            ZoeDuelResetLog("config loaded/reloaded");
    }

    void OnStartup() override
    {
        sDuelReset->LoadConfig(false);

        if (ZoeDuelResetEnabled())
            ZoeDuelResetLog("startup config loaded");
    }
};

class DuelResetScript : public PlayerScript
{
public:
    DuelResetScript() : PlayerScript("DuelResetScript", {
        PLAYERHOOK_ON_DUEL_START,
        PLAYERHOOK_ON_DUEL_END,
        PLAYERHOOK_ON_LOGOUT
    }) {}

    void OnPlayerDuelStart(Player* player1, Player* player2) override
    {
        if (!ZoeDuelResetEnabled())
            return;

        if (!player1 || !player2)
            return;

        if (ZoeDuelResetBlockedInstance(player1, player2))
        {
            ZoeDuelResetLog("blocked duel reset in battleground/arena");
            return;
        }

        if (!sDuelReset->IsAllowedInArea(player1))
        {
            if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.AnnounceBlockedArea", false))
            {
                ZoeDuelResetMessage(player1, "Duel Reset nao esta habilitado nesta area.");
                ZoeDuelResetMessage(player2, "Duel Reset nao esta habilitado nesta area.");
            }

            ZoeDuelResetLog("blocked duel reset by area player1='" + player1->GetName() + "' area=" + std::to_string(player1->GetAreaId()));
            return;
        }

        if (sDuelReset->GetResetCooldownsEnabled())
        {
            sDuelReset->SaveCooldownStateBeforeDuel(player1);
            sDuelReset->SaveCooldownStateBeforeDuel(player2);

            sDuelReset->ResetSpellCooldowns(player1, true);
            sDuelReset->ResetSpellCooldowns(player2, true);

            ZoeDuelResetResetConfiguredCooldowns(player1, "start");
            ZoeDuelResetResetConfiguredCooldowns(player2, "start");
        }

        if (sDuelReset->GetResetHealthEnabled())
        {
            sDuelReset->SaveHealthBeforeDuel(player1);
            sDuelReset->SaveManaBeforeDuel(player1);

            sDuelReset->SaveHealthBeforeDuel(player2);
            sDuelReset->SaveManaBeforeDuel(player2);

            ZoeDuelResetPreparePlayerForDuel(player1);
            ZoeDuelResetPreparePlayerForDuel(player2);
        }

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.RemoveDebuffs.OnStart", true))
        {
            ZoeDuelResetRemoveConfiguredDebuffs(player1, "start");
            ZoeDuelResetRemoveConfiguredDebuffs(player2, "start");
        }

        ZoeDuelResetMessage(player1, "Duel iniciado. Cooldowns, recursos e debuffs preparados.");
        ZoeDuelResetMessage(player2, "Duel iniciado. Cooldowns, recursos e debuffs preparados.");

        ZoeDuelResetLog("duel start player1='" + player1->GetName() + "' player2='" + player2->GetName() + "'");
    }

    void OnPlayerDuelEnd(Player* winner, Player* loser, DuelCompleteType type) override
    {
        if (!ZoeDuelResetEnabled())
            return;

        if (!winner || !loser)
            return;

        if (type == DUEL_WON)
        {
            if (sDuelReset->GetResetCooldownsEnabled())
            {
                sDuelReset->RestoreCooldownStateAfterDuel(winner);
                sDuelReset->RestoreCooldownStateAfterDuel(loser);

                if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.Cooldown.ExtraReset.AfterDuel", true))
                {
                    ZoeDuelResetResetConfiguredCooldowns(winner, "end");
                    ZoeDuelResetResetConfiguredCooldowns(loser, "end");
                }
            }

            if (sDuelReset->GetResetHealthEnabled())
            {
                if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.EndFullHealthMana", true))
                {
                    sDuelReset->ClearSavedState(winner);
                    sDuelReset->ClearSavedState(loser);

                    ZoeDuelResetFullHealthPower(winner);
                    ZoeDuelResetFullHealthPower(loser);
                }
                else
                {
                    sDuelReset->RestoreHealthAfterDuel(winner);
                    sDuelReset->RestoreHealthAfterDuel(loser);

                    sDuelReset->RestoreManaAfterDuel(winner);
                    sDuelReset->RestoreManaAfterDuel(loser);
                }
            }

            ZoeDuelResetCleanAfterDuel(winner);
            ZoeDuelResetCleanAfterDuel(loser);

            ZoeDuelResetAnnounceWinnerToZone(winner, loser);

            ZoeDuelResetMessage(winner, "Duel finalizado. Cooldowns, recursos e debuffs limpos.");
            ZoeDuelResetMessage(loser, "Duel finalizado. Cooldowns, recursos e debuffs limpos.");

            ZoeDuelResetLog("duel end winner='" + winner->GetName() + "' loser='" + loser->GetName() + "'");
            return;
        }

        if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.ClearStateOnInterruptedOrFled", true))
        {
            ZoeDuelResetClearPlayers(winner, loser);

            if (sConfigMgr->GetOption<bool>("DuelReset.Zoe.RemoveDebuffs.OnInterruptedOrFled", true))
            {
                ZoeDuelResetRemoveConfiguredDebuffs(winner, "interrupted");
                ZoeDuelResetRemoveConfiguredDebuffs(loser, "interrupted");
            }

            ZoeDuelResetLog("duel interrupted/fled state cleared player1='" + winner->GetName() + "' player2='" + loser->GetName() + "'");
        }
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        sDuelReset->ClearSavedState(player);
    }
};

void AddSC_DuelReset()
{
    new DuelResetAfterConfigLoad();
    new DuelResetScript();
}
