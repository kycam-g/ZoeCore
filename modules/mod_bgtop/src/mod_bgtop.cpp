/*
 * ZoeCore BGTop
 * Convertido de 02-Expert_BG_Announce.lua para módulo C++.
 */

#include "Battleground.h"
#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GossipDef.h"
#include "Log.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <ctime>
#include <sstream>
#include <string>
#include <unordered_set>

namespace
{
    uint32 BGTopProcessTimer = 0;
    uint32 BGTopResetTimer = 0;
    uint32 BGTopBackgroundTimer = 0;

    struct TopResult
    {
        uint32 Guid = 0;
        std::string Name = "Nenhum";
        uint32 Value = 0;
    };

    bool Enabled()
    {
        return sConfigMgr->GetOption<bool>("BGTop.Enable", true);
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

    std::string BGName(uint32 type)
    {
        switch (type)
        {
            case 1:  return "Alterac Valley";
            case 2:  return "Warsong Gulch";
            case 3:  return "Arathi Basin";
            case 7:  return "Eye of The Storm";
            case 9:  return "Strand of the Ancients";
            case 30: return "Isle of Conquest";
            default: return "Battleground";
        }
    }

    std::string WinnerText(uint32 winnerFaction)
    {
        if (winnerFaction == 1)
            return sConfigMgr->GetOption<std::string>("BGTop.Announce.WinnerAlliance", "|TInterface/PVPFrame/PVP-Currency-Alliance:18:18:0:-2|t|cff00ccffAlianca Win!|r");

        if (winnerFaction == 0)
            return sConfigMgr->GetOption<std::string>("BGTop.Announce.WinnerHorde", "|TInterface/PVPFrame/PVP-Currency-Horde:18:18:0:-2|t|cffff6060Horda Win!|r");

        return sConfigMgr->GetOption<std::string>("BGTop.Announce.WinnerUnknown", "|cffffffffSem vencedor|r");
    }

    void SendWorld(std::string const& message)
    {
        if (!sConfigMgr->GetOption<bool>("BGTop.Announce.World", true))
            return;

        std::string text = message;
        text = ReplaceAll(text, "\\n", "|n");
        text = ReplaceAll(text, "\n", "|n");

        // Em arquivo .conf, "\\\\" pode chegar como duas barras.
        // O client do WoW renderiza textura melhor com caminho normalizado.
        text = ReplaceAll(text, "\\\\", "\\");
        text = ReplaceAll(text, "\\", "/");

        uint32 sendMode = sConfigMgr->GetOption<uint32>("BGTop.Announce.SendMode", 1);

        std::size_t start = 0;
        while (start <= text.size())
        {
            std::size_t pos = text.find("|n", start);
            std::string line = pos == std::string::npos
                ? text.substr(start)
                : text.substr(start, pos - start);

            if (!line.empty())
            {
                if (sendMode == 1)
                {
                    WorldSessionMgr::SessionMap const& sessions = sWorldSessionMgr->GetAllSessions();
                    for (WorldSessionMgr::SessionMap::const_iterator itr = sessions.begin(); itr != sessions.end(); ++itr)
                    {
                        if (WorldSession* session = itr->second)
                        {
                            if (Player* player = session->GetPlayer())
                            {
                                if (player->IsInWorld())
                                    ChatHandler(player->GetSession()).SendSysMessage(line.c_str());
                            }
                        }
                    }
                }
                else
                {
                    sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, line);
                }
            }

            if (pos == std::string::npos)
                break;

            start = pos + 2;
        }
    }

    TopResult QueryTop(uint32 battlegroundId, std::string const& column)
    {
        TopResult top;

        std::string sql =
            "SELECT p.`character_guid`, COALESCE(c.`name`, 'Nenhum'), p.`" + column + "` "
            "FROM `pvpstats_players` p "
            "LEFT JOIN `characters` c ON c.`guid`=p.`character_guid` "
            "WHERE p.`battleground_id`=" + std::to_string(battlegroundId) + " "
            "ORDER BY p.`" + column + "` DESC LIMIT 1";

        if (QueryResult result = CharacterDatabase.Query(sql.c_str()))
        {
            Field* f = result->Fetch();
            top.Guid = f[0].Get<uint32>();
            top.Name = f[1].Get<std::string>();
            top.Value = f[2].Get<uint32>();
        }

        return top;
    }

    void UpsertAggregateTotal(uint32 battlegroundId)
    {
        CharacterDatabase.Execute(
            "INSERT INTO `zoecore_bgtop_stats_total` (`guid`, `player_name`, `kills`, `honorable_kills`, `deaths`, `damage`, `healing`, `honor`, `bgs_played`, `updated_at`) "
            "SELECT p.`character_guid`, COALESCE(c.`name`, 'Nenhum'), p.`score_killing_blows`, p.`score_honorable_kills`, p.`score_deaths`, p.`score_damage_done`, p.`score_healing_done`, p.`score_bonus_honor`, 1, UNIX_TIMESTAMP() "
            "FROM `pvpstats_players` p LEFT JOIN `characters` c ON c.`guid`=p.`character_guid` WHERE p.`battleground_id`={} "
            "ON DUPLICATE KEY UPDATE `player_name`=VALUES(`player_name`), `kills`=`kills`+VALUES(`kills`), `honorable_kills`=`honorable_kills`+VALUES(`honorable_kills`), `deaths`=`deaths`+VALUES(`deaths`), `damage`=`damage`+VALUES(`damage`), `healing`=`healing`+VALUES(`healing`), `honor`=`honor`+VALUES(`honor`), `bgs_played`=`bgs_played`+1, `updated_at`=UNIX_TIMESTAMP()",
            battlegroundId);
    }

    void UpsertAggregateWeekly(uint32 battlegroundId)
    {
        CharacterDatabase.Execute(
            "INSERT INTO `zoecore_bgtop_stats_weekly` (`guid`, `week_year`, `week_number`, `player_name`, `kills`, `honorable_kills`, `deaths`, `damage`, `healing`, `honor`, `bgs_played`, `updated_at`) "
            "SELECT p.`character_guid`, YEAR(CURDATE()), WEEK(CURDATE(), 3), COALESCE(c.`name`, 'Nenhum'), p.`score_killing_blows`, p.`score_honorable_kills`, p.`score_deaths`, p.`score_damage_done`, p.`score_healing_done`, p.`score_bonus_honor`, 1, UNIX_TIMESTAMP() "
            "FROM `pvpstats_players` p LEFT JOIN `characters` c ON c.`guid`=p.`character_guid` WHERE p.`battleground_id`={} "
            "ON DUPLICATE KEY UPDATE `player_name`=VALUES(`player_name`), `kills`=`kills`+VALUES(`kills`), `honorable_kills`=`honorable_kills`+VALUES(`honorable_kills`), `deaths`=`deaths`+VALUES(`deaths`), `damage`=`damage`+VALUES(`damage`), `healing`=`healing`+VALUES(`healing`), `honor`=`honor`+VALUES(`honor`), `bgs_played`=`bgs_played`+1, `updated_at`=UNIX_TIMESTAMP()",
            battlegroundId);
    }

    void UpsertAggregateMonthly(uint32 battlegroundId)
    {
        CharacterDatabase.Execute(
            "INSERT INTO `zoecore_bgtop_stats_monthly` (`guid`, `month_year`, `month_number`, `player_name`, `kills`, `honorable_kills`, `deaths`, `damage`, `healing`, `honor`, `bgs_played`, `updated_at`) "
            "SELECT p.`character_guid`, YEAR(CURDATE()), MONTH(CURDATE()), COALESCE(c.`name`, 'Nenhum'), p.`score_killing_blows`, p.`score_honorable_kills`, p.`score_deaths`, p.`score_damage_done`, p.`score_healing_done`, p.`score_bonus_honor`, 1, UNIX_TIMESTAMP() "
            "FROM `pvpstats_players` p LEFT JOIN `characters` c ON c.`guid`=p.`character_guid` WHERE p.`battleground_id`={} "
            "ON DUPLICATE KEY UPDATE `player_name`=VALUES(`player_name`), `kills`=`kills`+VALUES(`kills`), `honorable_kills`=`honorable_kills`+VALUES(`honorable_kills`), `deaths`=`deaths`+VALUES(`deaths`), `damage`=`damage`+VALUES(`damage`), `healing`=`healing`+VALUES(`healing`), `honor`=`honor`+VALUES(`honor`), `bgs_played`=`bgs_played`+1, `updated_at`=UNIX_TIMESTAMP()",
            battlegroundId);
    }

    std::string BuildAnnounceMessage(std::string const& bg, uint32 winnerFaction, TopResult const& topKill, TopResult const& topDamage, TopResult const& topHeal, TopResult const& topHK, TopResult const& topHonor, TopResult const& topDeaths)
    {
        std::string msg = sConfigMgr->GetOption<std::string>(
            "BGTop.Announce.Message",
            "|cffFFA500[BG Status - {bg}]: {winner} |cffff0000Top Kill: {top_kill} ({top_kill_value})|r |cffD2691ETop Damage: {top_damage} ({top_damage_value})|r |cff00FF7FTop Heal: {top_heal} ({top_heal_value})|r |cff6495EDTop Honor: {top_honor} ({top_honor_value})|r");

        msg = ReplaceAll(msg, "{bg}", bg);
        msg = ReplaceAll(msg, "{winner}", WinnerText(winnerFaction));
        msg = ReplaceAll(msg, "{top_kill}", topKill.Name);
        msg = ReplaceAll(msg, "{top_kill_value}", std::to_string(topKill.Value));
        msg = ReplaceAll(msg, "{top_damage}", topDamage.Name);
        msg = ReplaceAll(msg, "{top_damage_value}", std::to_string(topDamage.Value));
        msg = ReplaceAll(msg, "{top_heal}", topHeal.Name);
        msg = ReplaceAll(msg, "{top_heal_value}", std::to_string(topHeal.Value));
        msg = ReplaceAll(msg, "{top_hk}", topHK.Name);
        msg = ReplaceAll(msg, "{top_hk_value}", std::to_string(topHK.Value));
        msg = ReplaceAll(msg, "{top_honor}", topHonor.Name);
        msg = ReplaceAll(msg, "{top_honor_value}", std::to_string(topHonor.Value));
        msg = ReplaceAll(msg, "{top_deaths}", topDeaths.Name);
        msg = ReplaceAll(msg, "{top_deaths_value}", std::to_string(topDeaths.Value));
        return msg;
    }

    bool ProcessBattleground(uint32 battlegroundId)
    {
        QueryResult bgResult = CharacterDatabase.Query("SELECT `type`, `winner_faction` FROM `pvpstats_battlegrounds` WHERE `id`={}", battlegroundId);
        if (!bgResult)
            return false;

        Field* bgFields = bgResult->Fetch();
        uint32 bgType = bgFields[0].Get<uint32>();
        uint32 winnerFaction = bgFields[1].Get<uint32>();
        std::string bgName = BGName(bgType);

        TopResult topKill = QueryTop(battlegroundId, "score_killing_blows");
        TopResult topDamage = QueryTop(battlegroundId, "score_damage_done");
        TopResult topHeal = QueryTop(battlegroundId, "score_healing_done");
        TopResult topHK = QueryTop(battlegroundId, "score_honorable_kills");
        TopResult topHonor = QueryTop(battlegroundId, "score_bonus_honor");
        TopResult topDeaths = QueryTop(battlegroundId, "score_deaths");

        UpsertAggregateTotal(battlegroundId);

        if (sConfigMgr->GetOption<bool>("BGTop.Ranking.Weekly.Enable", true))
            UpsertAggregateWeekly(battlegroundId);

        if (sConfigMgr->GetOption<bool>("BGTop.Ranking.Monthly.Enable", true))
            UpsertAggregateMonthly(battlegroundId);

        CharacterDatabase.Execute(
            "REPLACE INTO `zoecore_bgtop_match_summary` (`battleground_id`, `bg_type`, `bg_name`, `winner_faction`, `top_kill_name`, `top_kill_value`, `top_damage_name`, `top_damage_value`, `top_heal_name`, `top_heal_value`, `top_hk_name`, `top_hk_value`, `top_honor_name`, `top_honor_value`, `top_deaths_name`, `top_deaths_value`, `created_at`) "
            "VALUES ({}, {}, '{}', {}, '{}', {}, '{}', {}, '{}', {}, '{}', {}, '{}', {}, '{}', {}, UNIX_TIMESTAMP())",
            battlegroundId, bgType, bgName, winnerFaction,
            topKill.Name, topKill.Value, topDamage.Name, topDamage.Value, topHeal.Name, topHeal.Value,
            topHK.Name, topHK.Value, topHonor.Name, topHonor.Value, topDeaths.Name, topDeaths.Value);

        CharacterDatabase.Execute("REPLACE INTO `zoecore_bgtop_processed` (`battleground_id`, `processed_at`) VALUES ({}, UNIX_TIMESTAMP())", battlegroundId);

        if (sConfigMgr->GetOption<bool>("BGTop.Announce.Enable", true))
            SendWorld(BuildAnnounceMessage(bgName, winnerFaction, topKill, topDamage, topHeal, topHK, topHonor, topDeaths));

        if (sConfigMgr->GetOption<bool>("BGTop.Log.Enable", true))
            LOG_INFO("module", "BGTop: BG {} processada/anunciada. Type={}, Winner={}", battlegroundId, bgType, winnerFaction);

        return true;
    }

    void ProcessPendingBattlegrounds()
    {
        uint32 maxCount = sConfigMgr->GetOption<uint32>("BGTop.Process.MaxBattlegroundsPerCycle", 10);

        QueryResult result = CharacterDatabase.Query(
            "SELECT b.`id` FROM `pvpstats_battlegrounds` b "
            "LEFT JOIN `zoecore_bgtop_processed` z ON z.`battleground_id`=b.`id` "
            "WHERE z.`battleground_id` IS NULL "
            "AND EXISTS (SELECT 1 FROM `pvpstats_players` p WHERE p.`battleground_id`=b.`id`) "
            "ORDER BY b.`id` ASC LIMIT {}", maxCount);

        if (!result)
            return;

        do
        {
            uint32 bgId = result->Fetch()[0].Get<uint32>();
            ProcessBattleground(bgId);
        } while (result->NextRow());
    }

    void ScheduleProcess()
    {
        if (!Enabled())
            return;

        uint32 delay = sConfigMgr->GetOption<uint32>("BGTop.Process.DelayMs", 1500);
        if (BGTopProcessTimer == 0 || BGTopProcessTimer > delay)
            BGTopProcessTimer = delay ? delay : 1;
    }

    void SendTopList(ChatHandler& chat, std::string const& title, std::string const& table, std::string const& column, std::string const& where)
    {
        uint32 limit = sConfigMgr->GetOption<uint32>("BGTop.Ranking.ShowLimit", 10);

        chat.PSendSysMessage("|cff00FFFF===== {} =====|r", title);

        std::string sql =
            "SELECT `player_name`, `" + column + "`, `bgs_played` FROM `" + table + "` " + where +
            " ORDER BY `" + column + "` DESC LIMIT " + std::to_string(limit);

        QueryResult result = CharacterDatabase.Query(sql.c_str());
        if (!result)
        {
            chat.SendSysMessage("Nenhum dado encontrado.");
            return;
        }

        uint32 pos = 1;
        do
        {
            Field* f = result->Fetch();
            chat.PSendSysMessage("{} - {} | {}: {} | BGs: {}", pos, f[0].Get<std::string>(), column, f[1].Get<uint32>(), f[2].Get<uint32>());
            ++pos;
        } while (result->NextRow());
    }

    void SendLastBG(ChatHandler& chat)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT `bg_name`, `winner_faction`, `top_kill_name`, `top_kill_value`, `top_damage_name`, `top_damage_value`, `top_heal_name`, `top_heal_value`, `top_hk_name`, `top_hk_value`, `top_honor_name`, `top_honor_value`, `top_deaths_name`, `top_deaths_value` "
            "FROM `zoecore_bgtop_match_summary` ORDER BY `battleground_id` DESC LIMIT 1");

        if (!result)
        {
            chat.SendSysMessage("Nenhuma BG processada ainda.");
            return;
        }

        Field* f = result->Fetch();
        chat.PSendSysMessage("|cff00FFFF===== Ultima BG - {} =====|r", f[0].Get<std::string>());
        chat.PSendSysMessage("Vencedor: {}", WinnerText(f[1].Get<uint32>()));
        chat.PSendSysMessage("Top Kill: {} ({})", f[2].Get<std::string>(), f[3].Get<uint32>());
        chat.PSendSysMessage("Top Damage: {} ({})", f[4].Get<std::string>(), f[5].Get<uint32>());
        chat.PSendSysMessage("Top Heal: {} ({})", f[6].Get<std::string>(), f[7].Get<uint32>());
        chat.PSendSysMessage("Top HK: {} ({})", f[8].Get<std::string>(), f[9].Get<uint32>());
        chat.PSendSysMessage("Top Honor: {} ({})", f[10].Get<std::string>(), f[11].Get<uint32>());
        chat.PSendSysMessage("Top Deaths: {} ({})", f[12].Get<std::string>(), f[13].Get<uint32>());
    }

    uint32 DaysInMonth(uint32 year, uint32 month)
    {
        static uint32 const days[] = { 31,28,31,30,31,30,31,31,30,31,30,31 };
        if (month == 2)
        {
            bool leap = (year % 400 == 0) || (year % 4 == 0 && year % 100 != 0);
            return leap ? 29 : 28;
        }

        if (month < 1 || month > 12)
            return 30;

        return days[month - 1];
    }

    std::string FormatDateTime(std::time_t value)
    {
        std::tm* tm = std::localtime(&value);
        if (!tm)
            return "Indefinido";

        char buffer[32];
        std::strftime(buffer, sizeof(buffer), "%d/%m/%Y %H:%M", tm);
        return buffer;
    }

    std::string GetNextWeeklyResetText()
    {
        std::time_t now = std::time(nullptr);
        std::tm target = *std::localtime(&now);

        uint32 cfgDay = sConfigMgr->GetOption<uint32>("BGTop.Reset.Weekly.DayOfWeek", 2);
        uint32 cfgHour = sConfigMgr->GetOption<uint32>("BGTop.Reset.Weekly.Hour", 0);
        uint32 cfgMinute = sConfigMgr->GetOption<uint32>("BGTop.Reset.Weekly.Minute", 0);

        if (cfgDay < 1 || cfgDay > 7)
            cfgDay = 2;

        uint32 currentDay = target.tm_wday + 1;
        uint32 addDays = (cfgDay + 7 - currentDay) % 7;

        target.tm_hour = cfgHour;
        target.tm_min = cfgMinute;
        target.tm_sec = 0;

        std::time_t targetTime = std::mktime(&target);
        if (addDays > 0)
        {
            target.tm_mday += addDays;
            targetTime = std::mktime(&target);
        }

        if (targetTime <= now)
        {
            target.tm_mday += 7;
            targetTime = std::mktime(&target);
        }

        return FormatDateTime(targetTime);
    }

    std::string GetNextMonthlyResetText()
    {
        std::time_t now = std::time(nullptr);
        std::tm target = *std::localtime(&now);

        uint32 cfgDay = sConfigMgr->GetOption<uint32>("BGTop.Reset.Monthly.DayOfMonth", 1);
        uint32 cfgHour = sConfigMgr->GetOption<uint32>("BGTop.Reset.Monthly.Hour", 0);
        uint32 cfgMinute = sConfigMgr->GetOption<uint32>("BGTop.Reset.Monthly.Minute", 0);

        uint32 year = uint32(target.tm_year + 1900);
        uint32 month = uint32(target.tm_mon + 1);

        if (cfgDay < 1)
            cfgDay = 1;

        uint32 maxDay = DaysInMonth(year, month);
        if (cfgDay > maxDay)
            cfgDay = maxDay;

        target.tm_mday = cfgDay;
        target.tm_hour = cfgHour;
        target.tm_min = cfgMinute;
        target.tm_sec = 0;

        std::time_t targetTime = std::mktime(&target);
        if (targetTime <= now)
        {
            month++;
            if (month > 12)
            {
                month = 1;
                year++;
            }

            target.tm_year = int(year - 1900);
            target.tm_mon = int(month - 1);
            target.tm_mday = std::min<uint32>(cfgDay, DaysInMonth(year, month));
            targetTime = std::mktime(&target);
        }

        return FormatDateTime(targetTime);
    }

    void SendInfo(ChatHandler& chat)
    {
        chat.SendSysMessage("|cff00FFFF===== BG Top ZoeCore =====|r");
        chat.PSendSysMessage("{}: |cffFFA500{}|r", sConfigMgr->GetOption<std::string>("BGTop.Text.NextWeeklyReset", "Proximo reset semanal"), GetNextWeeklyResetText());
        chat.PSendSysMessage("{}: |cffFFA500{}|r", sConfigMgr->GetOption<std::string>("BGTop.Text.NextMonthlyReset", "Proximo reset mensal"), GetNextMonthlyResetText());
        chat.SendSysMessage("Comandos: .bgtop info | total | weekly | monthly | last | process");
        chat.SendSysMessage("Categorias no NPC: kills, damage, healing, honorable kills, honor e deaths.");
    }

    void SendWorldMessageConfig(std::string const& key, std::string const& fallback)
    {
        SendWorld(sConfigMgr->GetOption<std::string>(key, fallback));
    }

    void ResetWeekly()
    {
        CharacterDatabase.Execute("DELETE FROM `zoecore_bgtop_stats_weekly` WHERE `week_year`=YEAR(CURDATE()) AND `week_number`=WEEK(CURDATE(), 3)");
        CharacterDatabase.Execute("REPLACE INTO `zoecore_bgtop_period_state` (`period`, `year`, `period_number`, `last_reset_at`) VALUES ('weekly', YEAR(CURDATE()), WEEK(CURDATE(), 3), UNIX_TIMESTAMP())");

        if (sConfigMgr->GetOption<bool>("BGTop.Reset.Weekly.Announce", true))
            SendWorldMessageConfig("BGTop.Reset.Weekly.Message", "|cffFFA500[BG Top]|r Ranking semanal de BG foi resetado!");
    }

    void ResetMonthly()
    {
        CharacterDatabase.Execute("DELETE FROM `zoecore_bgtop_stats_monthly` WHERE `month_year`=YEAR(CURDATE()) AND `month_number`=MONTH(CURDATE())");
        CharacterDatabase.Execute("REPLACE INTO `zoecore_bgtop_period_state` (`period`, `year`, `period_number`, `last_reset_at`) VALUES ('monthly', YEAR(CURDATE()), MONTH(CURDATE()), UNIX_TIMESTAMP())");

        if (sConfigMgr->GetOption<bool>("BGTop.Reset.Monthly.Announce", true))
            SendWorldMessageConfig("BGTop.Reset.Monthly.Message", "|cffFFA500[BG Top]|r Ranking mensal de BG foi resetado!");
    }

    void CheckReset()
    {
        QueryResult nowResult = CharacterDatabase.Query("SELECT YEAR(CURDATE()), WEEK(CURDATE(), 3), MONTH(CURDATE()), DAYOFWEEK(CURDATE()), DAYOFMONTH(CURDATE()), HOUR(NOW()), MINUTE(NOW())");
        if (!nowResult)
            return;

        Field* f = nowResult->Fetch();
        uint32 year = f[0].Get<uint32>();
        uint32 week = f[1].Get<uint32>();
        uint32 month = f[2].Get<uint32>();
        uint32 dayOfWeek = f[3].Get<uint32>();
        uint32 dayOfMonth = f[4].Get<uint32>();
        uint32 hour = f[5].Get<uint32>();
        uint32 minute = f[6].Get<uint32>();

        if (sConfigMgr->GetOption<bool>("BGTop.Reset.Weekly.Enable", true))
        {
            uint32 cfgDay = sConfigMgr->GetOption<uint32>("BGTop.Reset.Weekly.DayOfWeek", 2);
            uint32 cfgHour = sConfigMgr->GetOption<uint32>("BGTop.Reset.Weekly.Hour", 0);
            uint32 cfgMinute = sConfigMgr->GetOption<uint32>("BGTop.Reset.Weekly.Minute", 0);
            bool due = (dayOfWeek == cfgDay && (hour > cfgHour || (hour == cfgHour && minute >= cfgMinute)));
            bool done = false;

            if (QueryResult state = CharacterDatabase.Query("SELECT `year`, `period_number` FROM `zoecore_bgtop_period_state` WHERE `period`='weekly'"))
            {
                Field* s = state->Fetch();
                done = (s[0].Get<uint32>() == year && s[1].Get<uint32>() == week);
            }

            if (due && !done)
                ResetWeekly();
        }

        if (sConfigMgr->GetOption<bool>("BGTop.Reset.Monthly.Enable", true))
        {
            uint32 cfgDay = sConfigMgr->GetOption<uint32>("BGTop.Reset.Monthly.DayOfMonth", 1);
            uint32 cfgHour = sConfigMgr->GetOption<uint32>("BGTop.Reset.Monthly.Hour", 0);
            uint32 cfgMinute = sConfigMgr->GetOption<uint32>("BGTop.Reset.Monthly.Minute", 0);
            bool due = (dayOfMonth == cfgDay && (hour > cfgHour || (hour == cfgHour && minute >= cfgMinute)));
            bool done = false;

            if (QueryResult state = CharacterDatabase.Query("SELECT `year`, `period_number` FROM `zoecore_bgtop_period_state` WHERE `period`='monthly'"))
            {
                Field* s = state->Fetch();
                done = (s[0].Get<uint32>() == year && s[1].Get<uint32>() == month);
            }

            if (due && !done)
                ResetMonthly();
        }
    }
}

using namespace Acore::ChatCommands;

class BGTopCommandScript : public CommandScript
{
public:
    BGTopCommandScript() : CommandScript("BGTopCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable BGTopCommandTable =
        {
            { "info", HandleInfoCommand, SEC_PLAYER, Console::Yes },
            { "total", HandleTotalCommand, SEC_PLAYER, Console::Yes },
            { "weekly", HandleWeeklyCommand, SEC_PLAYER, Console::Yes },
            { "monthly", HandleMonthlyCommand, SEC_PLAYER, Console::Yes },
            { "last", HandleLastCommand, SEC_PLAYER, Console::Yes },
            { "process", HandleProcessCommand, SEC_GAMEMASTER, Console::Yes },
        };

        static ChatCommandTable BGTopCommandBaseTable =
        {
            { "bgtop", BGTopCommandTable },
        };

        return BGTopCommandBaseTable;
    }

    static bool HandleInfoCommand(ChatHandler* handler)
    {
        SendInfo(*handler);
        return true;
    }

    static bool HandleTotalCommand(ChatHandler* handler)
    {
        SendTopList(*handler, "Top BG Total - Kills", "zoecore_bgtop_stats_total", "kills", "");
        SendTopList(*handler, "Top BG Total - Damage", "zoecore_bgtop_stats_total", "damage", "");
        SendTopList(*handler, "Top BG Total - Healing", "zoecore_bgtop_stats_total", "healing", "");
        return true;
    }

    static bool HandleWeeklyCommand(ChatHandler* handler)
    {
        std::string where = "WHERE `week_year`=YEAR(CURDATE()) AND `week_number`=WEEK(CURDATE(), 3)";
        SendTopList(*handler, "Top BG Semanal - Kills", "zoecore_bgtop_stats_weekly", "kills", where);
        SendTopList(*handler, "Top BG Semanal - Damage", "zoecore_bgtop_stats_weekly", "damage", where);
        SendTopList(*handler, "Top BG Semanal - Healing", "zoecore_bgtop_stats_weekly", "healing", where);
        return true;
    }

    static bool HandleMonthlyCommand(ChatHandler* handler)
    {
        std::string where = "WHERE `month_year`=YEAR(CURDATE()) AND `month_number`=MONTH(CURDATE())";
        SendTopList(*handler, "Top BG Mensal - Kills", "zoecore_bgtop_stats_monthly", "kills", where);
        SendTopList(*handler, "Top BG Mensal - Damage", "zoecore_bgtop_stats_monthly", "damage", where);
        SendTopList(*handler, "Top BG Mensal - Healing", "zoecore_bgtop_stats_monthly", "healing", where);
        return true;
    }

    static bool HandleLastCommand(ChatHandler* handler)
    {
        SendLastBG(*handler);
        return true;
    }

    static bool HandleProcessCommand(ChatHandler* handler)
    {
        ProcessPendingBattlegrounds();
        handler->SendSysMessage("BGTop: processamento manual executado.");
        return true;
    }
};

class ZoeCoreBGTopPlayerScript : public PlayerScript
{
public:
    ZoeCoreBGTopPlayerScript() : PlayerScript("ZoeCoreBGTopPlayerScript", {
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGIN
    }) { }

    void OnPlayerLogin(Player* /*player*/) override
    {
        if (!Enabled())
            return;

        CheckReset();
        ScheduleProcess();
    }

    void OnPlayerUpdate(Player* /*player*/, uint32 diff) override
    {
        if (!Enabled())
            return;

        if (BGTopProcessTimer > 0)
        {
            if (diff >= BGTopProcessTimer)
            {
                BGTopProcessTimer = 0;
                ProcessPendingBattlegrounds();
            }
            else
            {
                BGTopProcessTimer -= diff;
            }
        }

        if (BGTopResetTimer <= diff)
        {
            BGTopResetTimer = sConfigMgr->GetOption<uint32>("BGTop.Reset.CheckIntervalSeconds", 60) * 1000;
            CheckReset();
        }
        else
        {
            BGTopResetTimer -= diff;
        }
    }
};

class ZoeCoreBGTopBGScript : public BGScript
{
public:
    ZoeCoreBGTopBGScript() : BGScript("ZoeCoreBGTopBGScript", {
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_END_REWARD
    }) { }

    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId /*winnerTeamId*/) override
    {
        if (!bg || !player || !Enabled())
            return;

        if (bg->isArena())
            return;

        ScheduleProcess();

        if (sConfigMgr->GetOption<bool>("BGTop.Log.Enable", true))
            LOG_INFO("module", "BGTop: fim de BG detectado, agendando processamento.");
    }
};


class ZoeCoreBGTopWorldScript : public WorldScript
{
public:
    ZoeCoreBGTopWorldScript() : WorldScript("ZoeCoreBGTopWorldScript", {
        WORLDHOOK_ON_UPDATE,
        WORLDHOOK_ON_STARTUP
    }) { }

    void OnStartup() override
    {
        if (!Enabled())
            return;

        ScheduleProcess();
        CheckReset();

        if (sConfigMgr->GetOption<bool>("BGTop.Log.Enable", true))
            LOG_INFO("module", "BGTop: WorldScript iniciado. Processamento automatico ativo.");
    }

    void OnUpdate(uint32 diff) override
    {
        if (!Enabled() || !sConfigMgr->GetOption<bool>("BGTop.Process.Background.Enable", true))
            return;

        if (BGTopBackgroundTimer <= diff)
        {
            BGTopBackgroundTimer = sConfigMgr->GetOption<uint32>("BGTop.Process.Background.IntervalSeconds", 5) * 1000;
            ProcessPendingBattlegrounds();
            CheckReset();
            return;
        }

        BGTopBackgroundTimer -= diff;
    }
};


class npc_bgtop : public CreatureScript
{
public:
    npc_bgtop() : CreatureScript("npc_bgtop") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !sConfigMgr->GetOption<bool>("BGTop.Npc.Enable", true))
            return false;

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Informacoes / Proximo Reset", GOSSIP_SENDER_MAIN, 1);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Top Total", GOSSIP_SENDER_MAIN, 2);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Top Semanal", GOSSIP_SENDER_MAIN, 3);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Top Mensal", GOSSIP_SENDER_MAIN, 4);
        AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Ultima BG", GOSSIP_SENDER_MAIN, 5);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Fechar", GOSSIP_SENDER_MAIN, 6);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ChatHandler chat(player->GetSession());

        switch (action)
        {
            case 1:
                SendInfo(chat);
                break;
            case 2:
                SendTopList(chat, "Top Total - Kills", "zoecore_bgtop_stats_total", "kills", "");
                SendTopList(chat, "Top Total - Damage", "zoecore_bgtop_stats_total", "damage", "");
                SendTopList(chat, "Top Total - Healing", "zoecore_bgtop_stats_total", "healing", "");
                SendTopList(chat, "Top Total - Honorable Kills", "zoecore_bgtop_stats_total", "honorable_kills", "");
                SendTopList(chat, "Top Total - Deaths", "zoecore_bgtop_stats_total", "deaths", "");
                break;
            case 3:
            {
                std::string where = "WHERE `week_year`=YEAR(CURDATE()) AND `week_number`=WEEK(CURDATE(), 3)";
                SendTopList(chat, "Top Semanal - Kills", "zoecore_bgtop_stats_weekly", "kills", where);
                SendTopList(chat, "Top Semanal - Damage", "zoecore_bgtop_stats_weekly", "damage", where);
                SendTopList(chat, "Top Semanal - Healing", "zoecore_bgtop_stats_weekly", "healing", where);
                SendTopList(chat, "Top Semanal - Honorable Kills", "zoecore_bgtop_stats_weekly", "honorable_kills", where);
                SendTopList(chat, "Top Semanal - Deaths", "zoecore_bgtop_stats_weekly", "deaths", where);
                break;
            }
            case 4:
            {
                std::string where = "WHERE `month_year`=YEAR(CURDATE()) AND `month_number`=MONTH(CURDATE())";
                SendTopList(chat, "Top Mensal - Kills", "zoecore_bgtop_stats_monthly", "kills", where);
                SendTopList(chat, "Top Mensal - Damage", "zoecore_bgtop_stats_monthly", "damage", where);
                SendTopList(chat, "Top Mensal - Healing", "zoecore_bgtop_stats_monthly", "healing", where);
                SendTopList(chat, "Top Mensal - Honorable Kills", "zoecore_bgtop_stats_monthly", "honorable_kills", where);
                SendTopList(chat, "Top Mensal - Deaths", "zoecore_bgtop_stats_monthly", "deaths", where);
                break;
            }
            case 5:
                SendLastBG(chat);
                break;
            default:
                break;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

void AddZoeCoreBGTopScripts()
{
    new BGTopCommandScript();
    new ZoeCoreBGTopPlayerScript();
    new ZoeCoreBGTopBGScript();
    new ZoeCoreBGTopWorldScript();
    new npc_bgtop();
}
