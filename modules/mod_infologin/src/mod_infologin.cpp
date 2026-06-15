/*
 * ZoeCore InfoLogin
 * Convertido de Lua para módulo C++.
 */

#include "Chat.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Language.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include <sstream>
#include <string>
#include <unordered_map>

namespace
{
    std::unordered_map<uint32, uint32> InfoLoginPending;

    struct InfoLoginCharacterData
    {
        uint32 totalKills = 0;
        uint32 honor = 0;
        uint32 arena = 0;
        uint32 totalTime = 0;
    };

    bool IsEnabled()
    {
        return sConfigMgr->GetOption<bool>("InfoLogin.Enable", true);
    }

    std::string ReplaceAll(std::string text, std::string const& from, std::string const& to)
    {
        if (from.empty())
            return text;

        std::size_t start = 0;
        while ((start = text.find(from, start)) != std::string::npos)
        {
            text.replace(start, from.length(), to);
            start += to.length();
        }

        return text;
    }

    std::string GetClassText(uint8 playerClass)
    {
        switch (playerClass)
        {
            case CLASS_WARRIOR:     return "|CFFC79C6EWarrior|r";
            case CLASS_PALADIN:     return "|CFFF58CBAPaladin|r";
            case CLASS_HUNTER:      return "|CFFABD473Hunter|r";
            case CLASS_ROGUE:       return "|CFFFFF569Rogue|r";
            case CLASS_PRIEST:      return "|CFFFFFFFFPriest|r";
            case CLASS_DEATH_KNIGHT:return "|CFFC41F3BDeath Knight|r";
            case CLASS_SHAMAN:      return "|CFF0070DEShaman|r";
            case CLASS_MAGE:        return "|CFF69CCF0Mage|r";
            case CLASS_WARLOCK:     return "|CFF9482C9Warlock|r";
            case CLASS_DRUID:       return "|CFFFF7D0ADruid|r";
            default:                return "|cffffffffUnknown|r";
        }
    }

    std::string GetFactionText(Player* player)
    {
        if (!player)
            return "";

        if (player->GetTeamId() == TEAM_HORDE)
            return "|TInterface\\PVPFrame\\PVP-Currency-Horde:18:18:0:-2|t|cffFF0000Horda|r";

        return "|TInterface\\PVPFrame\\PVP-Currency-Alliance:18:18:0:-2|t|cff3399FFAlianca|r";
    }

    std::string GetTeamColor(Player* player)
    {
        if (!player)
            return "|cffffffff";

        if (player->GetTeamId() == TEAM_HORDE)
            return "|TInterface\\PVPFrame\\PVP-Currency-Horde:18:18:0:-2|t|cffFF0000";

        return "|TInterface\\PVPFrame\\PVP-Currency-Alliance:18:18:0:-2|t|cff3399FF";
    }

    std::string FormatWorldMessage(Player* player, std::string message)
    {
        if (!player)
            return message;

        message = ReplaceAll(message, "{name}", player->GetName());
        message = ReplaceAll(message, "{class}", GetClassText(player->getClass()));
        message = ReplaceAll(message, "{faction}", GetFactionText(player));
        message = ReplaceAll(message, "{teamcolor}", GetTeamColor(player));
        return message;
    }

    void SendWorld(std::string const& message)
    {
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message);
    }

    void SendLine(Player* player, std::string const& text)
    {
        if (!player || !player->GetSession())
            return;

        std::string prefix = sConfigMgr->GetOption<std::string>("InfoLogin.LinePrefix", "|cff00FFFF• |r");
        ChatHandler(player->GetSession()).SendSysMessage((prefix + text).c_str());
    }

    std::string FormatTime(uint32 totalSeconds)
    {
        uint32 hours = totalSeconds / 3600;
        uint32 minutes = (totalSeconds / 60) % 60;
        uint32 seconds = totalSeconds % 60;

        std::ostringstream out;
        out << hours << " Horas " << minutes << " Minutos e " << seconds << " Segundos";
        return out.str();
    }

    InfoLoginCharacterData GetCharacterData(Player* player)
    {
        InfoLoginCharacterData data;

        if (!player)
            return data;

        if (QueryResult result = CharacterDatabase.Query("SELECT `totalKills`, `totalHonorPoints`, `arenaPoints`, `totaltime` FROM `characters` WHERE `guid`={}", player->GetGUID().GetCounter()))
        {
            Field* fields = result->Fetch();
            data.totalKills = fields[0].Get<uint32>();
            data.honor = fields[1].Get<uint32>();
            data.arena = fields[2].Get<uint32>();
            data.totalTime = fields[3].Get<uint32>();
        }

        return data;
    }

    std::string GetJoinDate(Player* player)
    {
        if (!player || !player->GetSession())
            return "Desconhecido";

        uint32 accountId = player->GetSession()->GetAccountId();

        if (QueryResult result = LoginDatabase.Query("SELECT DATE_FORMAT(`joindate`, '%d/%m/%Y') FROM `account` WHERE `id`={}", accountId))
            return result->Fetch()[0].Get<std::string>();

        return "Desconhecido";
    }

    std::string GetAccountType(Player* player)
    {
        if (!player || !player->GetSession())
            return sConfigMgr->GetOption<std::string>("InfoLogin.FreeName", "|cff9932CCGratis|r");

        if (player->GetSession()->IsGMAccount())
            return "|cffFF0000Game Master|r";

        uint32 vip1 = sConfigMgr->GetOption<uint32>("InfoLogin.Vip1.ItemEntry", 33564);
        uint32 vip2 = sConfigMgr->GetOption<uint32>("InfoLogin.Vip2.ItemEntry", 33565);

        if (vip1 && player->HasItemCount(vip1, 1, true))
            return sConfigMgr->GetOption<std::string>("InfoLogin.Vip1.Name", "|cffFA58D0VIP-1|r");

        if (vip2 && player->HasItemCount(vip2, 1, true))
            return sConfigMgr->GetOption<std::string>("InfoLogin.Vip2.Name", "|cffDC143CVIP-2|r");

        return sConfigMgr->GetOption<std::string>("InfoLogin.FreeName", "|cff9932CCGratis|r");
    }

    uint32 GetPvPKingScore(Player* player)
    {
        if (!player || !sConfigMgr->GetOption<bool>("InfoLogin.PvPKing.Enable", true))
            return 0;

        if (sConfigMgr->GetOption<bool>("InfoLogin.PvPKing.LegacyCharactersColumn.Enable", false))
        {
            // Use apenas se a coluna antiga characters.reidopvp existir.
            if (QueryResult result = CharacterDatabase.Query("SELECT `reidopvp` FROM `characters` WHERE `guid`={}", player->GetGUID().GetCounter()))
                return result->Fetch()[0].Get<uint32>();
        }

        if (QueryResult result = CharacterDatabase.Query("SELECT `score` FROM `zoecore_pvp_king_stats` WHERE `guid`={}", player->GetGUID().GetCounter()))
            return result->Fetch()[0].Get<uint32>();

        return 0;
    }

    std::string GetCurrentPvPKingTextFromTable(std::string const& tableName, std::string const& label)
    {
        std::string query = "SELECT `player_name`, `score` FROM `" + tableName + "` WHERE `id`=1 AND `player_guid` > 0";
        if (QueryResult result = CharacterDatabase.Query(query.c_str()))
        {
            Field* fields = result->Fetch();
            std::string name = fields[0].Get<std::string>();
            uint32 score = fields[1].Get<uint32>();

            std::ostringstream out;
            out << "|cffFFFFFF" << label << ": |cffFFA500" << name << " (" << score << ")|r";
            return out.str();
        }

        return "|cffFFFFFF" + label + ": |cffFFA500Nenhum|r";
    }

    std::string GetCurrentPvPKingText()
    {
        return GetCurrentPvPKingTextFromTable("zoecore_pvp_king", "Atual Rei do PvP");
    }

    std::string GetCurrentPvPKingWeeklyText()
    {
        return GetCurrentPvPKingTextFromTable("zoecore_pvp_king_weekly", "Rei do PvP Semanal");
    }

    std::string GetCurrentPvPKingMonthlyText()
    {
        return GetCurrentPvPKingTextFromTable("zoecore_pvp_king_monthly", "Rei do PvP Mensal");
    }

    uint32 GetPvPKingPeriodScore(Player* player, std::string const& tableName)
    {
        if (!player || !sConfigMgr->GetOption<bool>("InfoLogin.PvPKing.Enable", true))
            return 0;

        std::string query = "SELECT `score` FROM `" + tableName + "` WHERE `guid`=" + std::to_string(player->GetGUID().GetCounter()) + " ORDER BY `updated_at` DESC LIMIT 1";
        if (QueryResult result = CharacterDatabase.Query(query.c_str()))
            return result->Fetch()[0].Get<uint32>();

        return 0;
    }

    bool IsGMHidden(Player* player, std::string const& key)
    {
        if (!player || !player->GetSession())
            return true;

        return sConfigMgr->GetOption<bool>(key, true) && player->GetSession()->IsGMAccount();
    }

    void HandleFirstSeen(Player* player, InfoLoginCharacterData const& data)
    {
        if (!player || !sConfigMgr->GetOption<bool>("InfoLogin.Announce.NewPlayer.Enable", true))
            return;

        uint32 guid = player->GetGUID().GetCounter();
        uint32 accountId = player->GetSession()->GetAccountId();

        QueryResult seen = CharacterDatabase.Query("SELECT `guid` FROM `zoecore_infologin_seen` WHERE `guid`={}", guid);
        if (seen)
            return;

        CharacterDatabase.Execute("INSERT INTO `zoecore_infologin_seen` (`guid`, `account_id`, `first_seen_at`) VALUES ({}, {}, {})", guid, accountId, uint32(time(nullptr)));

        uint32 maxTime = sConfigMgr->GetOption<uint32>("InfoLogin.Announce.NewPlayer.MaxTotalTimeSeconds", 60);
        if (data.totalTime > maxTime)
            return;

        std::string message = sConfigMgr->GetOption<std::string>("InfoLogin.Announce.NewPlayer.Message", "{faction} tem um novo membro {name} ({class})");
        SendWorld(FormatWorldMessage(player, message));
    }

    void SendInfoLogin(Player* player)
    {
        if (!player || !player->GetSession() || !IsEnabled())
            return;

        InfoLoginCharacterData data = GetCharacterData(player);
        std::string name = player->GetName();

        HandleFirstSeen(player, data);

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.Welcome", true))
            SendLine(player, "|cffFFFFFFSeja bem vindo |cffFFA500" + name + "|r");

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.Kills", true))
            SendLine(player, "|cffFFFFFFKills: |cffFFA500" + std::to_string(data.totalKills) + "|r");

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.Honor", true))
            SendLine(player, "|cffFFFFFFHonor: |cffFFA500" + std::to_string(data.honor) + "|r");

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.Arena", true))
            SendLine(player, "|cffFFFFFFArena: |cffFFA500" + std::to_string(data.arena) + "|r");

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.AccountType", true))
            SendLine(player, "|cffFFFFFFConta: " + GetAccountType(player));

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.PvPKingScore", true))
            SendLine(player, "|cffFFFFFFRei do PvP: |cffFFA500" + std::to_string(GetPvPKingScore(player)) + "|r");

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.CurrentPvPKing", true))
        {
            std::string currentKing = GetCurrentPvPKingText();
            if (!currentKing.empty())
                SendLine(player, currentKing);
        }

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.PvPKingWeeklyScore", true))
            SendLine(player, "|cffFFFFFFRei do PvP Semanal: |cffFFA500" + std::to_string(GetPvPKingPeriodScore(player, "zoecore_pvp_king_stats_weekly")) + "|r");

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.CurrentPvPKingWeekly", true))
        {
            std::string currentKingWeekly = GetCurrentPvPKingWeeklyText();
            if (!currentKingWeekly.empty())
                SendLine(player, currentKingWeekly);
        }

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.PvPKingMonthlyScore", true))
            SendLine(player, "|cffFFFFFFRei do PvP Mensal: |cffFFA500" + std::to_string(GetPvPKingPeriodScore(player, "zoecore_pvp_king_stats_monthly")) + "|r");

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.CurrentPvPKingMonthly", true))
        {
            std::string currentKingMonthly = GetCurrentPvPKingMonthlyText();
            if (!currentKingMonthly.empty())
                SendLine(player, currentKingMonthly);
        }

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.JoinDate", true))
            SendLine(player, "|cffFFFFFFJogando desde: |cffFFA500" + GetJoinDate(player) + "|r");

        if (sConfigMgr->GetOption<bool>("InfoLogin.Show.TotalOnlineTime", true))
            SendLine(player, "|cffFFFFFFTempo Total Online: |cffFFA500" + FormatTime(data.totalTime) + "|r");
    }

    void SendOnlineAnnouncement(Player* player)
    {
        if (!player || !sConfigMgr->GetOption<bool>("InfoLogin.Announce.Online.Enable", true))
            return;

        if (IsGMHidden(player, "InfoLogin.Announce.Online.HideGM"))
            return;

        std::string message = sConfigMgr->GetOption<std::string>("InfoLogin.Announce.Online.Message", "{teamcolor}{name}|r acabou de entrar.");
        SendWorld(FormatWorldMessage(player, message));
    }

    void SendOfflineAnnouncement(Player* player)
    {
        if (!player || !sConfigMgr->GetOption<bool>("InfoLogin.Announce.Offline.Enable", false))
            return;

        if (IsGMHidden(player, "InfoLogin.Announce.Offline.HideGM"))
            return;

        std::string message = sConfigMgr->GetOption<std::string>("InfoLogin.Announce.Offline.Message", "{teamcolor}{name}|r agora esta offline.");
        SendWorld(FormatWorldMessage(player, message));
    }
}

class ZoeCoreInfoLoginPlayerScript : public PlayerScript
{
public:
    ZoeCoreInfoLoginPlayerScript() : PlayerScript("ZoeCoreInfoLoginPlayerScript", {
        PLAYERHOOK_ON_LOGIN,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_UPDATE
    }) { }

    void OnPlayerLogin(Player* player) override
    {
        if (!player || !IsEnabled())
            return;

        SendOnlineAnnouncement(player);

        uint32 delayMs = sConfigMgr->GetOption<uint32>("InfoLogin.DelayMs", 1000);
        if (delayMs == 0)
        {
            SendInfoLogin(player);
            return;
        }

        InfoLoginPending[player->GetGUID().GetCounter()] = delayMs;
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        InfoLoginPending.erase(player->GetGUID().GetCounter());

        if (IsEnabled())
            SendOfflineAnnouncement(player);
    }

    void OnPlayerUpdate(Player* player, uint32 diff) override
    {
        if (!player || !IsEnabled())
            return;

        uint32 guid = player->GetGUID().GetCounter();
        auto itr = InfoLoginPending.find(guid);
        if (itr == InfoLoginPending.end())
            return;

        if (diff >= itr->second)
        {
            InfoLoginPending.erase(itr);
            SendInfoLogin(player);
            return;
        }

        itr->second -= diff;
    }
};

void AddZoeCoreInfoLoginScripts()
{
    new ZoeCoreInfoLoginPlayerScript();
}
