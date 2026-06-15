/*
 * ZoeCore Rei do PvP
 * Ranking total/semanal/mensal por kills PvP.
 */

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "GossipDef.h"
#include "Map.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include <ctime>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace
{
    struct KillKey
    {
        uint32 Killer;
        uint32 Victim;

        bool operator==(KillKey const& other) const
        {
            return Killer == other.Killer && Victim == other.Victim;
        }
    };

    struct KillKeyHash
    {
        std::size_t operator()(KillKey const& key) const
        {
            return (std::size_t(key.Killer) << 32) ^ std::size_t(key.Victim);
        }
    };

    std::unordered_map<KillKey, uint32, KillKeyHash> LastKillTime;
    uint32 ResetCheckTimer = 0;

    bool IsEnabled()
    {
        return sConfigMgr->GetOption<bool>("ReiDoPvP.Enable", true);
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

    std::unordered_set<uint32> ParseIdList(std::string text)
    {
        std::unordered_set<uint32> ids;

        for (char& c : text)
            if (c == ',' || c == ';')
                c = ' ';

        std::stringstream ss(text);
        uint32 id;
        while (ss >> id)
            ids.insert(id);

        return ids;
    }

    bool IdInConfigList(std::string const& key, uint32 value)
    {
        std::string text = sConfigMgr->GetOption<std::string>(key, "");
        std::unordered_set<uint32> ids = ParseIdList(text);
        return ids.find(value) != ids.end();
    }

    bool IsAllowedMapZone(Player* killer)
    {
        if (!killer)
            return false;

        bool mapFilter = sConfigMgr->GetOption<bool>("ReiDoPvP.Count.Map.Enable", false);
        bool zoneFilter = sConfigMgr->GetOption<bool>("ReiDoPvP.Count.Zone.Enable", false);

        if (mapFilter && IdInConfigList("ReiDoPvP.Count.Map.Ids", killer->GetMapId()))
            return true;

        if (zoneFilter && IdInConfigList("ReiDoPvP.Count.Zone.Ids", killer->GetZoneId()))
            return true;

        if (mapFilter || zoneFilter)
            return false;

        return true;
    }

    bool IsAllowedContext(Player* killer)
    {
        if (!killer)
            return false;

        if (!IsAllowedMapZone(killer))
            return false;

        if (killer->InArena())
            return sConfigMgr->GetOption<bool>("ReiDoPvP.Count.Arena.Enable", true);

        if (killer->InBattleground())
            return sConfigMgr->GetOption<bool>("ReiDoPvP.Count.Battleground.Enable", true);

        return sConfigMgr->GetOption<bool>("ReiDoPvP.Count.World.Enable", true);
    }

    bool IsAntiFarmBlocked(Player* killer, Player* victim)
    {
        if (!killer || !victim)
            return true;

        if (!sConfigMgr->GetOption<bool>("ReiDoPvP.AntiFarm.Enable", true))
            return false;

        uint32 cooldown = sConfigMgr->GetOption<uint32>("ReiDoPvP.AntiFarm.CooldownSeconds", 300);
        uint32 now = uint32(time(nullptr));

        KillKey key { killer->GetGUID().GetCounter(), victim->GetGUID().GetCounter() };
        auto itr = LastKillTime.find(key);

        if (itr != LastKillTime.end() && now < itr->second + cooldown)
            return true;

        LastKillTime[key] = now;
        return false;
    }

    uint8 GetTeamValue(Player* player)
    {
        if (!player)
            return 0;

        return player->GetTeamId() == TEAM_HORDE ? 1 : 0;
    }

    void UpdateTotalStats(Player* player, uint32 score, bool isKill)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        uint32 account = player->GetSession()->GetAccountId();
        uint32 now = uint32(time(nullptr));

        if (isKill)
        {
            CharacterDatabase.Execute(
                "INSERT INTO `zoecore_pvp_king_stats` (`guid`,`account_id`,`player_name`,`class`,`race`,`team`,`score`,`kills`,`deaths`,`streak`,`best_streak`,`updated_at`) "
                "VALUES ({}, {}, '{}', {}, {}, {}, {}, 1, 0, 1, 1, {}) "
                "ON DUPLICATE KEY UPDATE `player_name`=VALUES(`player_name`), `class`=VALUES(`class`), `race`=VALUES(`race`), `team`=VALUES(`team`), `score`=`score`+{}, `kills`=`kills`+1, `streak`=`streak`+1, `best_streak`=GREATEST(`best_streak`, `streak`), `updated_at`={}",
                guid, account, player->GetName(), player->getClass(), player->getRace(), GetTeamValue(player), score, now, score, now);
        }
        else
        {
            CharacterDatabase.Execute(
                "INSERT INTO `zoecore_pvp_king_stats` (`guid`,`account_id`,`player_name`,`class`,`race`,`team`,`score`,`kills`,`deaths`,`streak`,`best_streak`,`updated_at`) "
                "VALUES ({}, {}, '{}', {}, {}, {}, 0, 0, 1, 0, 0, {}) "
                "ON DUPLICATE KEY UPDATE `player_name`=VALUES(`player_name`), `class`=VALUES(`class`), `race`=VALUES(`race`), `team`=VALUES(`team`), `deaths`=`deaths`+1, `streak`=0, `updated_at`={}",
                guid, account, player->GetName(), player->getClass(), player->getRace(), GetTeamValue(player), now, now);
        }
    }

    void UpdateWeeklyStats(Player* player, uint32 score, bool isKill)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        uint32 account = player->GetSession()->GetAccountId();
        uint32 now = uint32(time(nullptr));

        if (isKill)
        {
            CharacterDatabase.Execute(
                "INSERT INTO `zoecore_pvp_king_stats_weekly` (`guid`,`account_id`,`player_name`,`class`,`race`,`team`,`score`,`kills`,`deaths`,`streak`,`best_streak`,`week_year`,`week_number`,`updated_at`) "
                "VALUES ({}, {}, '{}', {}, {}, {}, {}, 1, 0, 1, 1, YEAR(CURDATE()), WEEK(CURDATE(), 3), {}) "
                "ON DUPLICATE KEY UPDATE `player_name`=VALUES(`player_name`), `class`=VALUES(`class`), `race`=VALUES(`race`), `team`=VALUES(`team`), `score`=`score`+{}, `kills`=`kills`+1, `streak`=`streak`+1, `best_streak`=GREATEST(`best_streak`, `streak`), `updated_at`={}",
                guid, account, player->GetName(), player->getClass(), player->getRace(), GetTeamValue(player), score, now, score, now);
        }
        else
        {
            CharacterDatabase.Execute(
                "INSERT INTO `zoecore_pvp_king_stats_weekly` (`guid`,`account_id`,`player_name`,`class`,`race`,`team`,`score`,`kills`,`deaths`,`streak`,`best_streak`,`week_year`,`week_number`,`updated_at`) "
                "VALUES ({}, {}, '{}', {}, {}, {}, 0, 0, 1, 0, 0, YEAR(CURDATE()), WEEK(CURDATE(), 3), {}) "
                "ON DUPLICATE KEY UPDATE `player_name`=VALUES(`player_name`), `class`=VALUES(`class`), `race`=VALUES(`race`), `team`=VALUES(`team`), `deaths`=`deaths`+1, `streak`=0, `updated_at`={}",
                guid, account, player->GetName(), player->getClass(), player->getRace(), GetTeamValue(player), now, now);
        }
    }

    void UpdateMonthlyStats(Player* player, uint32 score, bool isKill)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        uint32 account = player->GetSession()->GetAccountId();
        uint32 now = uint32(time(nullptr));

        if (isKill)
        {
            CharacterDatabase.Execute(
                "INSERT INTO `zoecore_pvp_king_stats_monthly` (`guid`,`account_id`,`player_name`,`class`,`race`,`team`,`score`,`kills`,`deaths`,`streak`,`best_streak`,`month_year`,`month_number`,`updated_at`) "
                "VALUES ({}, {}, '{}', {}, {}, {}, {}, 1, 0, 1, 1, YEAR(CURDATE()), MONTH(CURDATE()), {}) "
                "ON DUPLICATE KEY UPDATE `player_name`=VALUES(`player_name`), `class`=VALUES(`class`), `race`=VALUES(`race`), `team`=VALUES(`team`), `score`=`score`+{}, `kills`=`kills`+1, `streak`=`streak`+1, `best_streak`=GREATEST(`best_streak`, `streak`), `updated_at`={}",
                guid, account, player->GetName(), player->getClass(), player->getRace(), GetTeamValue(player), score, now, score, now);
        }
        else
        {
            CharacterDatabase.Execute(
                "INSERT INTO `zoecore_pvp_king_stats_monthly` (`guid`,`account_id`,`player_name`,`class`,`race`,`team`,`score`,`kills`,`deaths`,`streak`,`best_streak`,`month_year`,`month_number`,`updated_at`) "
                "VALUES ({}, {}, '{}', {}, {}, {}, 0, 0, 1, 0, 0, YEAR(CURDATE()), MONTH(CURDATE()), {}) "
                "ON DUPLICATE KEY UPDATE `player_name`=VALUES(`player_name`), `class`=VALUES(`class`), `race`=VALUES(`race`), `team`=VALUES(`team`), `deaths`=`deaths`+1, `streak`=0, `updated_at`={}",
                guid, account, player->GetName(), player->getClass(), player->getRace(), GetTeamValue(player), now, now);
        }
    }

    void UpdateKingTable(std::string const& statsTable, std::string const& kingTable, std::string const& periodWhere, std::string const& periodColumns, std::string const& periodSelect)
    {
        std::string sql =
            "REPLACE INTO `" + kingTable + "` (`id`, " + periodColumns + "`player_guid`, `account_id`, `player_name`, `class`, `race`, `team`, `score`, `updated_at`) "
            "SELECT 1, " + periodSelect + "`guid`, `account_id`, `player_name`, `class`, `race`, `team`, `score`, UNIX_TIMESTAMP() "
            "FROM `" + statsTable + "` " + periodWhere + " ORDER BY `score` DESC, `kills` DESC, `best_streak` DESC LIMIT 1";

        CharacterDatabase.Execute(sql.c_str());
    }

    uint32 GetTrainerEntryForClass(uint8 playerClass)
    {
        switch (playerClass)
        {
            case CLASS_WARRIOR:      return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.Warrior", 26332);
            case CLASS_PALADIN:      return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.Paladin", 26327);
            case CLASS_HUNTER:       return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.Hunter", 26325);
            case CLASS_ROGUE:        return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.Rogue", 26329);
            case CLASS_PRIEST:       return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.Priest", 26328);
            case CLASS_DEATH_KNIGHT: return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.DeathKnight", 29195);
            case CLASS_SHAMAN:       return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.Shaman", 26330);
            case CLASS_MAGE:         return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.Mage", 26326);
            case CLASS_WARLOCK:      return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.Warlock", 26331);
            case CLASS_DRUID:        return sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.TrainerModel.Druid", 26324);
            default:                 return 0;
        }
    }

    uint32 GetDisplayForWinnerClass(uint8 playerClass)
    {
        uint32 fallback = sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.ModelFallback", 25901);

        if (!sConfigMgr->GetOption<bool>("ReiDoPvP.Npc.UseClassTrainerModel", true))
            return fallback;

        uint32 trainerEntry = GetTrainerEntryForClass(playerClass);
        if (!trainerEntry)
            return fallback;

        if (QueryResult result = WorldDatabase.Query("SELECT `CreatureDisplayID` FROM `creature_template_model` WHERE `CreatureID`={} ORDER BY `Idx` ASC LIMIT 1", trainerEntry))
            return result->Fetch()[0].Get<uint32>();

        return fallback;
    }

    void UpdateNpcTemplateFromCurrentKing()
    {
        if (!sConfigMgr->GetOption<bool>("ReiDoPvP.Npc.UpdateTemplate.Enable", true))
            return;

        uint32 npcEntry = sConfigMgr->GetOption<uint32>("ReiDoPvP.Npc.Entry", 500090);
        if (!npcEntry)
            return;

        QueryResult result = CharacterDatabase.Query("SELECT `player_name`, `class` FROM `zoecore_pvp_king` WHERE `id`=1 AND `player_guid` > 0");
        if (!result)
            return;

        Field* fields = result->Fetch();
        std::string winnerName = fields[0].Get<std::string>();
        uint8 winnerClass = fields[1].Get<uint8>();
        std::string subName = sConfigMgr->GetOption<std::string>("ReiDoPvP.Npc.UpdateTemplate.SubName", "Rei do PvP");
        uint32 displayId = GetDisplayForWinnerClass(winnerClass);

        WorldDatabase.Execute("UPDATE `creature_template` SET `name`='{}', `subname`='{}' WHERE `entry`={}", winnerName, subName, npcEntry);

        if (displayId)
        {
            WorldDatabase.Execute("DELETE FROM `creature_template_model` WHERE `CreatureID`={}", npcEntry);
            WorldDatabase.Execute("INSERT INTO `creature_template_model` (`CreatureID`, `Idx`, `CreatureDisplayID`, `DisplayScale`, `Probability`, `VerifiedBuild`) VALUES ({}, 0, {}, 1, 1, 0)", npcEntry, displayId);
        }
    }

    void RefreshKingTables()
    {
        UpdateKingTable("zoecore_pvp_king_stats", "zoecore_pvp_king", "", "", "");
        UpdateKingTable("zoecore_pvp_king_stats_weekly", "zoecore_pvp_king_weekly",
            "WHERE `week_year`=YEAR(CURDATE()) AND `week_number`=WEEK(CURDATE(), 3) ",
            "`week_year`, `week_number`, ",
            "`week_year`, `week_number`, ");
        UpdateKingTable("zoecore_pvp_king_stats_monthly", "zoecore_pvp_king_monthly",
            "WHERE `month_year`=YEAR(CURDATE()) AND `month_number`=MONTH(CURDATE()) ",
            "`month_year`, `month_number`, ",
            "`month_year`, `month_number`, ");

        UpdateNpcTemplateFromCurrentKing();
    }

    void LogKill(Player* killer, Player* victim, uint32 score)
    {
        if (!killer || !victim)
            return;

        CharacterDatabase.Execute(
            "INSERT INTO `zoecore_pvp_king_kill_log` (`killer_guid`, `victim_guid`, `killer_name`, `victim_name`, `map`, `zone`, `area`, `score`, `created_at`) "
            "VALUES ({}, {}, '{}', '{}', {}, {}, {}, {}, {})",
            killer->GetGUID().GetCounter(), victim->GetGUID().GetCounter(), killer->GetName(), victim->GetName(),
            killer->GetMapId(), killer->GetZoneId(), killer->GetAreaId(), score, uint32(time(nullptr)));
    }

    void CountPvPKill(Player* killer, Player* victim)
    {
        if (!killer || !victim || killer == victim || !IsEnabled())
            return;

        if (sConfigMgr->GetOption<bool>("ReiDoPvP.IgnoreGM", true))
        {
            if (killer->GetSession()->IsGMAccount() || victim->GetSession()->IsGMAccount())
                return;
        }

        if (!IsAllowedContext(killer))
            return;

        if (IsAntiFarmBlocked(killer, victim))
            return;

        uint32 score = sConfigMgr->GetOption<uint32>("ReiDoPvP.Score.Kill", 1);

        UpdateTotalStats(killer, score, true);
        UpdateWeeklyStats(killer, score, true);
        UpdateMonthlyStats(killer, score, true);

        if (sConfigMgr->GetOption<bool>("ReiDoPvP.Count.Death", true))
        {
            UpdateTotalStats(victim, 0, false);
            UpdateWeeklyStats(victim, 0, false);
            UpdateMonthlyStats(victim, 0, false);
        }

        LogKill(killer, victim, score);
        RefreshKingTables();
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

        uint32 cfgDay = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Weekly.DayOfWeek", 2);
        uint32 cfgHour = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Weekly.Hour", 0);
        uint32 cfgMinute = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Weekly.Minute", 0);

        if (cfgDay < 1 || cfgDay > 7)
            cfgDay = 2;

        uint32 currentDay = target.tm_wday + 1; // tm_wday: 0 domingo, config: 1 domingo
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

        uint32 cfgDay = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Monthly.DayOfMonth", 1);
        uint32 cfgHour = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Monthly.Hour", 0);
        uint32 cfgMinute = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Monthly.Minute", 0);

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

    std::string RewardConfigPrefix(std::string const& period)
    {
        return period == "weekly" ? "ReiDoPvP.Reward.Weekly." : "ReiDoPvP.Reward.Monthly.";
    }

    bool RewardEnabled(std::string const& period)
    {
        return sConfigMgr->GetOption<bool>(RewardConfigPrefix(period) + "Enable", true);
    }

    void QueueRewardFromKingTable(std::string const& period, std::string const& kingTable)
    {
        if (!RewardEnabled(period))
            return;

        std::string prefix = RewardConfigPrefix(period);
        uint32 minScore = sConfigMgr->GetOption<uint32>(prefix + "MinScore", 1);
        uint32 itemEntry = sConfigMgr->GetOption<uint32>(prefix + "ItemEntry", 900001);
        uint32 itemCount = sConfigMgr->GetOption<uint32>(prefix + "ItemCount", period == "weekly" ? 50 : 200);
        uint32 honor = sConfigMgr->GetOption<uint32>(prefix + "Honor", period == "weekly" ? 500 : 2000);
        uint32 arena = sConfigMgr->GetOption<uint32>(prefix + "Arena", period == "weekly" ? 0 : 100);

        std::string periodYearCol = period == "weekly" ? "`week_year`" : "`month_year`";
        std::string periodNumberCol = period == "weekly" ? "`week_number`" : "`month_number`";

        std::string sql = "SELECT " + periodYearCol + ", " + periodNumberCol + ", `player_guid`, `account_id`, `player_name`, `score` FROM `" + kingTable + "` WHERE `id`=1 AND `player_guid` > 0 AND `score` >= " + std::to_string(minScore);
        QueryResult result = CharacterDatabase.Query(sql.c_str());
        if (!result)
            return;

        Field* fields = result->Fetch();
        uint32 periodYear = fields[0].Get<uint32>();
        uint32 periodNumber = fields[1].Get<uint32>();
        uint32 guid = fields[2].Get<uint32>();
        uint32 accountId = fields[3].Get<uint32>();
        std::string name = fields[4].Get<std::string>();
        uint32 score = fields[5].Get<uint32>();

        if (!itemEntry)
            itemCount = 0;

        if (!itemCount && !honor && !arena)
            return;

        CharacterDatabase.Execute(
            "INSERT IGNORE INTO `zoecore_pvp_king_reward_pending` (`period`, `period_year`, `period_number`, `guid`, `account_id`, `player_name`, `score`, `item_entry`, `item_count`, `honor`, `arena`, `created_at`, `delivered`) "
            "VALUES ('{}', {}, {}, {}, {}, '{}', {}, {}, {}, {}, {}, {}, 0)",
            period, periodYear, periodNumber, guid, accountId, name, score, itemEntry, itemCount, honor, arena, uint32(time(nullptr)));
    }

    void DeliverPendingRewards(Player* player)
    {
        if (!player || !sConfigMgr->GetOption<bool>("ReiDoPvP.Reward.DeliverOnLogin", true))
            return;

        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `period`, `item_entry`, `item_count`, `honor`, `arena` FROM `zoecore_pvp_king_reward_pending` WHERE `guid`={} AND `delivered`=0 ORDER BY `id` ASC",
            player->GetGUID().GetCounter());

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            uint32 rewardId = fields[0].Get<uint32>();
            std::string period = fields[1].Get<std::string>();
            uint32 itemEntry = fields[2].Get<uint32>();
            uint32 itemCount = fields[3].Get<uint32>();
            uint32 honor = fields[4].Get<uint32>();
            uint32 arena = fields[5].Get<uint32>();

            if (itemEntry && itemCount)
            {
                if (!player->AddItem(itemEntry, itemCount))
                {
                    ChatHandler(player->GetSession()).SendSysMessage("|cffff2020[Rei do PvP]|r Recompensa pendente, mas seu inventario esta cheio.");
                    continue;
                }
            }

            if (honor)
                player->ModifyHonorPoints(int32(honor));

            if (arena)
                player->ModifyArenaPoints(int32(arena));

            CharacterDatabase.Execute("UPDATE `zoecore_pvp_king_reward_pending` SET `delivered`=1, `delivered_at`={} WHERE `id`={}", uint32(time(nullptr)), rewardId);

            std::string message = period == "weekly"
                ? sConfigMgr->GetOption<std::string>("ReiDoPvP.Reward.Weekly.DeliveredMessage", "|cff00FFFF[Rei do PvP]|r Voce recebeu sua recompensa semanal do Rei do PvP!")
                : sConfigMgr->GetOption<std::string>("ReiDoPvP.Reward.Monthly.DeliveredMessage", "|cff00FFFF[Rei do PvP]|r Voce recebeu sua recompensa mensal do Rei do PvP!");

            ChatHandler(player->GetSession()).SendSysMessage(message.c_str());

        } while (result->NextRow());
    }


    void SendWorld(std::string const& message)
    {
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message);
    }

    void ResetWeekly()
    {
        QueueRewardFromKingTable("weekly", "zoecore_pvp_king_weekly");

        CharacterDatabase.Execute("DELETE FROM `zoecore_pvp_king_stats_weekly` WHERE `week_year`=YEAR(CURDATE()) AND `week_number`=WEEK(CURDATE(), 3)");
        CharacterDatabase.Execute("REPLACE INTO `zoecore_pvp_king_weekly` (`id`, `week_year`, `week_number`, `player_guid`, `account_id`, `player_name`, `class`, `race`, `team`, `score`, `updated_at`) VALUES (1, YEAR(CURDATE()), WEEK(CURDATE(), 3), 0, 0, 'Nenhum', 0, 0, 0, 0, UNIX_TIMESTAMP())");
        CharacterDatabase.Execute("REPLACE INTO `zoecore_pvp_king_period_state` (`period`, `year`, `period_number`, `last_reset_at`) VALUES ('weekly', YEAR(CURDATE()), WEEK(CURDATE(), 3), UNIX_TIMESTAMP())");

        if (sConfigMgr->GetOption<bool>("ReiDoPvP.Reset.Weekly.Announce", true))
            SendWorld(sConfigMgr->GetOption<std::string>("ReiDoPvP.Reset.Weekly.Message", "|cffff2020[ZoeCore]|r Ranking semanal do Rei do PvP foi resetado!"));
    }

    void ResetMonthly()
    {
        QueueRewardFromKingTable("monthly", "zoecore_pvp_king_monthly");

        CharacterDatabase.Execute("DELETE FROM `zoecore_pvp_king_stats_monthly` WHERE `month_year`=YEAR(CURDATE()) AND `month_number`=MONTH(CURDATE())");
        CharacterDatabase.Execute("REPLACE INTO `zoecore_pvp_king_monthly` (`id`, `month_year`, `month_number`, `player_guid`, `account_id`, `player_name`, `class`, `race`, `team`, `score`, `updated_at`) VALUES (1, YEAR(CURDATE()), MONTH(CURDATE()), 0, 0, 'Nenhum', 0, 0, 0, 0, UNIX_TIMESTAMP())");
        CharacterDatabase.Execute("REPLACE INTO `zoecore_pvp_king_period_state` (`period`, `year`, `period_number`, `last_reset_at`) VALUES ('monthly', YEAR(CURDATE()), MONTH(CURDATE()), UNIX_TIMESTAMP())");

        if (sConfigMgr->GetOption<bool>("ReiDoPvP.Reset.Monthly.Announce", true))
            SendWorld(sConfigMgr->GetOption<std::string>("ReiDoPvP.Reset.Monthly.Message", "|cffff2020[ZoeCore]|r Ranking mensal do Rei do PvP foi resetado!"));
    }

    void CheckPeriodReset()
    {
        if (!IsEnabled())
            return;

        QueryResult nowResult = CharacterDatabase.Query("SELECT YEAR(CURDATE()), WEEK(CURDATE(), 3), MONTH(CURDATE()), DAYOFWEEK(CURDATE()), DAYOFMONTH(CURDATE()), HOUR(NOW()), MINUTE(NOW())");
        if (!nowResult)
            return;

        Field* fields = nowResult->Fetch();
        uint32 year = fields[0].Get<uint32>();
        uint32 week = fields[1].Get<uint32>();
        uint32 month = fields[2].Get<uint32>();
        uint32 dayOfWeek = fields[3].Get<uint32>();
        uint32 dayOfMonth = fields[4].Get<uint32>();
        uint32 hour = fields[5].Get<uint32>();
        uint32 minute = fields[6].Get<uint32>();

        if (sConfigMgr->GetOption<bool>("ReiDoPvP.Reset.Weekly.Enable", true))
        {
            uint32 cfgDay = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Weekly.DayOfWeek", 2);
            uint32 cfgHour = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Weekly.Hour", 0);
            uint32 cfgMinute = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Weekly.Minute", 0);

            bool due = (dayOfWeek == cfgDay && (hour > cfgHour || (hour == cfgHour && minute >= cfgMinute)));
            bool alreadyThisPeriod = false;

            if (QueryResult state = CharacterDatabase.Query("SELECT `year`, `period_number` FROM `zoecore_pvp_king_period_state` WHERE `period`='weekly'"))
            {
                Field* s = state->Fetch();
                alreadyThisPeriod = (s[0].Get<uint32>() == year && s[1].Get<uint32>() == week);
            }

            if (due && !alreadyThisPeriod)
                ResetWeekly();
        }

        if (sConfigMgr->GetOption<bool>("ReiDoPvP.Reset.Monthly.Enable", true))
        {
            uint32 cfgDay = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Monthly.DayOfMonth", 1);
            uint32 cfgHour = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Monthly.Hour", 0);
            uint32 cfgMinute = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.Monthly.Minute", 0);

            bool due = (dayOfMonth == cfgDay && (hour > cfgHour || (hour == cfgHour && minute >= cfgMinute)));
            bool alreadyThisPeriod = false;

            if (QueryResult state = CharacterDatabase.Query("SELECT `year`, `period_number` FROM `zoecore_pvp_king_period_state` WHERE `period`='monthly'"))
            {
                Field* s = state->Fetch();
                alreadyThisPeriod = (s[0].Get<uint32>() == year && s[1].Get<uint32>() == month);
            }

            if (due && !alreadyThisPeriod)
                ResetMonthly();
        }
    }

    std::string GetKingLine(std::string const& table, std::string const& label)
    {
        std::string sql = "SELECT `player_name`, `score` FROM `" + table + "` WHERE `id`=1 AND `player_guid` > 0";
        if (QueryResult result = CharacterDatabase.Query(sql.c_str()))
        {
            Field* fields = result->Fetch();
            std::ostringstream out;
            out << "|cffFFFFFF" << label << ": |cffFFA500" << fields[0].Get<std::string>() << " (" << fields[1].Get<uint32>() << ")|r";
            return out.str();
        }

        return "|cffFFFFFF" + label + ": |cffFFA500Nenhum|r";
    }

    void SendTopList(Player* player, std::string const& title, std::string const& table, std::string const& where)
    {
        if (!player)
            return;

        ChatHandler chat(player->GetSession());
        chat.PSendSysMessage("|cff00FFFF===== {} =====|r", title);

        std::string sql = "SELECT `player_name`, `score`, `kills`, `deaths`, `best_streak` FROM `" + table + "` " + where + " ORDER BY `score` DESC, `kills` DESC, `best_streak` DESC LIMIT 10";
        QueryResult result = CharacterDatabase.Query(sql.c_str());
        if (!result)
        {
            chat.SendSysMessage("Nenhum jogador no ranking ainda.");
            return;
        }

        uint32 pos = 1;
        do
        {
            Field* fields = result->Fetch();
            chat.PSendSysMessage("{} - {} | Score: {} | Kills: {} | Deaths: {} | Best Streak: {}",
                pos, fields[0].Get<std::string>(), fields[1].Get<uint32>(), fields[2].Get<uint32>(), fields[3].Get<uint32>(), fields[4].Get<uint32>());
            ++pos;
        } while (result->NextRow());
    }

    void SendMyStats(Player* player)
    {
        if (!player)
            return;

        uint32 guid = player->GetGUID().GetCounter();
        ChatHandler chat(player->GetSession());

        chat.SendSysMessage("|cff00FFFF===== Meu Rei do PvP =====|r");

        if (QueryResult result = CharacterDatabase.Query("SELECT `score`, `kills`, `deaths`, `streak`, `best_streak` FROM `zoecore_pvp_king_stats` WHERE `guid`={}", guid))
        {
            Field* f = result->Fetch();
            chat.PSendSysMessage("Geral: Score {} | Kills {} | Deaths {} | Streak {} | Best {}",
                f[0].Get<uint32>(), f[1].Get<uint32>(), f[2].Get<uint32>(), f[3].Get<uint32>(), f[4].Get<uint32>());
        }
        else
        {
            chat.SendSysMessage("Geral: sem pontuacao.");
        }

        if (QueryResult result = CharacterDatabase.Query("SELECT `score`, `kills`, `deaths`, `streak`, `best_streak` FROM `zoecore_pvp_king_stats_weekly` WHERE `guid`={} AND `week_year`=YEAR(CURDATE()) AND `week_number`=WEEK(CURDATE(), 3)", guid))
        {
            Field* f = result->Fetch();
            chat.PSendSysMessage("Semanal: Score {} | Kills {} | Deaths {} | Streak {} | Best {}",
                f[0].Get<uint32>(), f[1].Get<uint32>(), f[2].Get<uint32>(), f[3].Get<uint32>(), f[4].Get<uint32>());
        }
        else
        {
            chat.SendSysMessage("Semanal: sem pontuacao.");
        }

        if (QueryResult result = CharacterDatabase.Query("SELECT `score`, `kills`, `deaths`, `streak`, `best_streak` FROM `zoecore_pvp_king_stats_monthly` WHERE `guid`={} AND `month_year`=YEAR(CURDATE()) AND `month_number`=MONTH(CURDATE())", guid))
        {
            Field* f = result->Fetch();
            chat.PSendSysMessage("Mensal: Score {} | Kills {} | Deaths {} | Streak {} | Best {}",
                f[0].Get<uint32>(), f[1].Get<uint32>(), f[2].Get<uint32>(), f[3].Get<uint32>(), f[4].Get<uint32>());
        }
        else
        {
            chat.SendSysMessage("Mensal: sem pontuacao.");
        }
    }

    void SendKingInfo(Player* player)
    {
        if (!player)
            return;

        ChatHandler chat(player->GetSession());
        chat.SendSysMessage("|cff00FFFF===== Rei do PvP =====|r");
        chat.SendSysMessage(GetKingLine("zoecore_pvp_king", "Top Rei do PvP").c_str());
        chat.SendSysMessage(GetKingLine("zoecore_pvp_king_weekly", "Rei do PvP Semanal").c_str());
        chat.SendSysMessage(GetKingLine("zoecore_pvp_king_monthly", "Rei do PvP Mensal").c_str());

        chat.PSendSysMessage("|cffFFFFFFProximo fechamento semanal: |cffFFA500{}|r", GetNextWeeklyResetText());
        chat.PSendSysMessage("|cffFFFFFFProximo fechamento mensal: |cffFFA500{}|r", GetNextMonthlyResetText());

        chat.PSendSysMessage("|cffFFFFFFPremiacao semanal: |cffFFA500{}|r",
            sConfigMgr->GetOption<std::string>("ReiDoPvP.Reward.Weekly.Description", "50 Fragmentos Cruéis + 500 Honor"));

        chat.PSendSysMessage("|cffFFFFFFPremiacao mensal: |cffFFA500{}|r",
            sConfigMgr->GetOption<std::string>("ReiDoPvP.Reward.Monthly.Description", "200 Fragmentos Cruéis + 2000 Honor + 100 Arena Points"));

        chat.SendSysMessage(sConfigMgr->GetOption<std::string>("ReiDoPvP.Npc.RewardText.Total", "Top Rei do PvP: prestigio eterno no ranking geral.").c_str());
        chat.SendSysMessage(sConfigMgr->GetOption<std::string>("ReiDoPvP.Npc.RewardText.Weekly", "Rei do PvP Semanal: vencedor anunciado semanalmente.").c_str());
        chat.SendSysMessage(sConfigMgr->GetOption<std::string>("ReiDoPvP.Npc.RewardText.Monthly", "Rei do PvP Mensal: vencedor anunciado mensalmente.").c_str());
    }
}

class ZoeCoreReiDoPvPPlayerScript : public PlayerScript
{
public:
    ZoeCoreReiDoPvPPlayerScript() : PlayerScript("ZoeCoreReiDoPvPPlayerScript", {
        PLAYERHOOK_ON_PVP_KILL, // chama OnPlayerPVPKill no AzerothCore
        PLAYERHOOK_ON_UPDATE,
        PLAYERHOOK_ON_LOGIN
    }) { }

    void OnPlayerPVPKill(Player* killer, Player* killed) override
    {
        CountPvPKill(killer, killed);
        CheckPeriodReset();
    }

    void OnPlayerLogin(Player* player) override
    {
        CheckPeriodReset();
        DeliverPendingRewards(player);
    }

    void OnPlayerUpdate(Player* /*player*/, uint32 diff) override
    {
        if (!IsEnabled())
            return;

        if (ResetCheckTimer <= diff)
        {
            ResetCheckTimer = sConfigMgr->GetOption<uint32>("ReiDoPvP.Reset.CheckIntervalSeconds", 60) * 1000;
            CheckPeriodReset();
            return;
        }

        ResetCheckTimer -= diff;
    }
};

class npc_reidopvp : public CreatureScript
{
public:
    npc_reidopvp() : CreatureScript("npc_reidopvp") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !sConfigMgr->GetOption<bool>("ReiDoPvP.Npc.Enable", true))
            return false;

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Informacoes do Rei do PvP", GOSSIP_SENDER_MAIN, 1);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Top Rei do PvP", GOSSIP_SENDER_MAIN, 2);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Top Semanal", GOSSIP_SENDER_MAIN, 3);
        AddGossipItemFor(player, GOSSIP_ICON_BATTLE, "Top Mensal", GOSSIP_SENDER_MAIN, 4);
        AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Minhas Estatisticas", GOSSIP_SENDER_MAIN, 5);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Fechar", GOSSIP_SENDER_MAIN, 6);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        ClearGossipMenuFor(player);

        switch (action)
        {
            case 1:
                SendKingInfo(player);
                break;
            case 2:
                SendTopList(player, "Top Rei do PvP", "zoecore_pvp_king_stats", "");
                break;
            case 3:
                SendTopList(player, "Top Rei do PvP Semanal", "zoecore_pvp_king_stats_weekly", "WHERE `week_year`=YEAR(CURDATE()) AND `week_number`=WEEK(CURDATE(), 3)");
                break;
            case 4:
                SendTopList(player, "Top Rei do PvP Mensal", "zoecore_pvp_king_stats_monthly", "WHERE `month_year`=YEAR(CURDATE()) AND `month_number`=MONTH(CURDATE())");
                break;
            case 5:
                SendMyStats(player);
                break;
            default:
                break;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

void AddZoeCoreReiDoPvPScripts()
{
    new ZoeCoreReiDoPvPPlayerScript();
    new npc_reidopvp();
}
