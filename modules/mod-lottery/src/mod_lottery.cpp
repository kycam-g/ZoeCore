/*
 * ZoeCore Lottery V2
 * Mega-Sena + Numero da Sorte + Jackpot + Historico + Ranking.
 */

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DatabaseEnv.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "Random.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "WorldSession.h"
#include "WorldSessionMgr.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace
{
    enum LotteryAction : uint32
    {
        ACTION_INFO              = 100,
        ACTION_BUY_MEGA_AUTO     = 101,
        ACTION_BUY_LUCKY_AUTO    = 102,
        ACTION_MY_TICKETS        = 103,
        ACTION_CLAIM             = 104,
        ACTION_HISTORY           = 105,
        ACTION_RANKING           = 106,
        ACTION_BUY_MEGA_MANUAL   = 107,
        ACTION_BUY_LUCKY_MANUAL  = 108,
        ACTION_BACK              = 109,
        ACTION_ACTIVE_DRAWS      = 110,
        ACTION_JACKPOTS          = 111,
    };

    struct PrizeItem
    {
        uint32 ItemEntry;
        uint32 Count;
        std::string Source;
    };

    struct TicketInfo
    {
        uint32 TicketId;
        uint32 Guid;
        std::string PlayerName;
        std::string NumbersText;
        uint32 Matches;
    };

    uint32 WorldTickTimer = 30000;
    std::set<std::string> SentWarnings;

    bool LotteryEnabled()
    {
        return sConfigMgr->GetOption<bool>("Lottery.Enable", true);
    }

    uint32 Now()
    {
        return uint32(time(nullptr));
    }

    std::string Prefix()
    {
        return sConfigMgr->GetOption<std::string>("Lottery.Message.Prefix", "|cff00FFFF[ZoeCore Loteria]|r");
    }

    void SendPlayer(Player* player, std::string const& message)
    {
        if (!player || !player->GetSession())
            return;

        ChatHandler(player->GetSession()).SendSysMessage(message.c_str());
    }

    void Announce(std::string const& message)
    {
        if (!sConfigMgr->GetOption<bool>("Lottery.Announce.World.Enable", true))
            return;

        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message);
    }

    std::string NormalizeNumberText(std::string text)
    {
        for (char& c : text)
        {
            if (c == ';' || c == '|' || c == '/' || c == '-' || c == '.' || c == ' ')
                c = ',';
        }

        return text;
    }

    std::vector<uint32> ParseNumbers(std::string text)
    {
        text = NormalizeNumberText(text);

        std::vector<uint32> out;
        std::stringstream stream(text);
        std::string token;

        while (std::getline(stream, token, ','))
        {
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);

            if (token.empty())
                continue;

            try
            {
                out.push_back(static_cast<uint32>(std::stoul(token)));
            }
            catch (...)
            {
                continue;
            }
        }

        std::sort(out.begin(), out.end());
        out.erase(std::unique(out.begin(), out.end()), out.end());
        return out;
    }

    std::string JoinNumbers(std::vector<uint32> numbers)
    {
        std::sort(numbers.begin(), numbers.end());

        std::ostringstream ss;
        for (std::size_t i = 0; i < numbers.size(); ++i)
        {
            if (i)
                ss << ",";
            ss << numbers[i];
        }

        return ss.str();
    }

    std::vector<uint32> GenerateUniqueNumbers(uint32 minNumber, uint32 maxNumber, uint32 count)
    {
        std::vector<uint32> pool;

        if (minNumber > maxNumber)
            std::swap(minNumber, maxNumber);

        for (uint32 i = minNumber; i <= maxNumber; ++i)
            pool.push_back(i);

        for (std::size_t i = 0; i < pool.size(); ++i)
        {
            std::size_t j = urand(i, pool.size() - 1);
            std::swap(pool[i], pool[j]);
        }

        if (count > pool.size())
            count = pool.size();

        std::vector<uint32> result(pool.begin(), pool.begin() + count);
        std::sort(result.begin(), result.end());
        return result;
    }

    uint32 CountMatches(std::vector<uint32> const& a, std::vector<uint32> const& b)
    {
        uint32 count = 0;

        for (uint32 n : a)
        {
            if (std::find(b.begin(), b.end(), n) != b.end())
                ++count;
        }

        return count;
    }

    std::vector<uint32> ParseMinuteList(std::string text)
    {
        std::vector<uint32> out = ParseNumbers(text);
        std::sort(out.begin(), out.end(), std::greater<uint32>());
        return out;
    }

    std::vector<PrizeItem> ParsePrizeItems(std::string text, std::string const& source)
    {
        std::vector<PrizeItem> prizes;
        std::stringstream stream(text);
        std::string token;

        while (std::getline(stream, token, ','))
        {
            token.erase(0, token.find_first_not_of(" \t\r\n"));
            token.erase(token.find_last_not_of(" \t\r\n") + 1);

            if (token.empty())
                continue;

            std::size_t pos = token.find(':');
            if (pos == std::string::npos)
                continue;

            try
            {
                uint32 item = static_cast<uint32>(std::stoul(token.substr(0, pos)));
                uint32 count = static_cast<uint32>(std::stoul(token.substr(pos + 1)));

                if (item && count)
                    prizes.push_back({ item, count, source });
            }
            catch (...)
            {
                continue;
            }
        }

        return prizes;
    }

    std::vector<PrizeItem> ConfigPrizeItems(std::string const& listKey, std::string const& oldItemKey, std::string const& oldCountKey, std::string const& source)
    {
        std::string text = sConfigMgr->GetOption<std::string>(listKey, "");
        std::vector<PrizeItem> prizes = ParsePrizeItems(text, source);

        if (!prizes.empty())
            return prizes;

        uint32 item = sConfigMgr->GetOption<uint32>(oldItemKey, 0);
        uint32 count = sConfigMgr->GetOption<uint32>(oldCountKey, 0);

        if (item && count)
            prizes.push_back({ item, count, source });

        return prizes;
    }

    uint32 GetActiveRoundId(std::string const& type)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT `id` FROM `zoecore_lottery_rounds` WHERE `type`='{}' AND `status`=0 ORDER BY `id` DESC LIMIT 1",
            type);

        if (!result)
            return 0;

        return (*result)[0].Get<uint32>();
    }

    void EnsureJackpot(std::string const& type)
    {
        uint32 item = 0;
        uint32 start = 0;

        if (type == "MEGA")
        {
            item = sConfigMgr->GetOption<uint32>("Lottery.Mega.Jackpot.ItemEntry", 900001);
            start = sConfigMgr->GetOption<uint32>("Lottery.Mega.Jackpot.StartCount", 500);
        }
        else
        {
            item = sConfigMgr->GetOption<uint32>("Lottery.Lucky.Jackpot.ItemEntry", 900001);
            start = sConfigMgr->GetOption<uint32>("Lottery.Lucky.Jackpot.StartCount", 250);
        }

        QueryResult result = CharacterDatabase.Query("SELECT `type` FROM `zoecore_lottery_jackpot` WHERE `type`='{}'", type);
        if (result)
            return;

        CharacterDatabase.Execute(
            "INSERT INTO `zoecore_lottery_jackpot` (`type`, `item_entry`, `current_count`, `start_count`, `updated_at`) VALUES ('{}', {}, {}, {}, {})",
            type, item, start, start, Now());
    }

    void ResetJackpot(std::string const& type)
    {
        EnsureJackpot(type);

        uint32 item = 0;
        uint32 start = 0;

        if (type == "MEGA")
        {
            item = sConfigMgr->GetOption<uint32>("Lottery.Mega.Jackpot.ItemEntry", 900001);
            start = sConfigMgr->GetOption<uint32>("Lottery.Mega.Jackpot.StartCount", 500);
        }
        else
        {
            item = sConfigMgr->GetOption<uint32>("Lottery.Lucky.Jackpot.ItemEntry", 900001);
            start = sConfigMgr->GetOption<uint32>("Lottery.Lucky.Jackpot.StartCount", 250);
        }

        CharacterDatabase.Execute(
            "UPDATE `zoecore_lottery_jackpot` SET `item_entry`={}, `current_count`={}, `start_count`={}, `updated_at`={} WHERE `type`='{}'",
            item, start, start, Now(), type);
    }

    std::pair<uint32, uint32> GetJackpot(std::string const& type)
    {
        EnsureJackpot(type);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `item_entry`, `current_count` FROM `zoecore_lottery_jackpot` WHERE `type`='{}'",
            type);

        if (!result)
            return { 0, 0 };

        return { (*result)[0].Get<uint32>(), (*result)[1].Get<uint32>() };
    }

    void AddToJackpot(std::string const& type, uint32 ticketCost)
    {
        bool enabled = false;
        uint32 percent = 0;

        if (type == "MEGA")
        {
            enabled = sConfigMgr->GetOption<bool>("Lottery.Mega.Jackpot.Enable", true);
            percent = sConfigMgr->GetOption<uint32>("Lottery.Mega.Jackpot.PercentFromTicketCost", 80);
        }
        else
        {
            enabled = sConfigMgr->GetOption<bool>("Lottery.Lucky.Jackpot.Enable", true);
            percent = sConfigMgr->GetOption<uint32>("Lottery.Lucky.Jackpot.PercentFromTicketCost", 80);
        }

        if (!enabled || !ticketCost || !percent)
            return;

        EnsureJackpot(type);

        uint32 addCount = (ticketCost * percent) / 100;
        if (!addCount)
            addCount = 1;

        CharacterDatabase.Execute(
            "UPDATE `zoecore_lottery_jackpot` SET `current_count`=`current_count`+{}, `updated_at`={} WHERE `type`='{}'",
            addCount, Now(), type);
    }

    uint32 CreateRound(std::string const& type)
    {
        EnsureJackpot(type);

        uint32 now = Now();
        uint32 intervalMinutes = type == "MEGA"
            ? sConfigMgr->GetOption<uint32>("Lottery.Mega.DrawIntervalMinutes", 1440)
            : sConfigMgr->GetOption<uint32>("Lottery.Lucky.DrawIntervalMinutes", 1440);

        uint32 drawAt = now + intervalMinutes * 60;

        CharacterDatabase.Execute(
            "INSERT INTO `zoecore_lottery_rounds` (`type`, `status`, `created_at`, `draw_at`) VALUES ('{}', 0, {}, {})",
            type, now, drawAt);

        QueryResult result = CharacterDatabase.Query("SELECT LAST_INSERT_ID()");
        if (!result)
            return 0;

        return (*result)[0].Get<uint32>();
    }

    uint32 EnsureActiveRound(std::string const& type)
    {
        uint32 roundId = GetActiveRoundId(type);
        if (roundId)
            return roundId;

        return CreateRound(type);
    }

    uint32 GetCurrencyItemEntry()
    {
        return sConfigMgr->GetOption<uint32>("Lottery.Currency.ItemEntry", 900001);
    }

    uint32 GetTicketCost(std::string const& type)
    {
        if (type == "MEGA")
            return sConfigMgr->GetOption<uint32>("Lottery.Currency.Mega.ItemCount",
                sConfigMgr->GetOption<uint32>("Lottery.Mega.TicketCostCount", 10));

        return sConfigMgr->GetOption<uint32>("Lottery.Currency.Lucky.ItemCount",
            sConfigMgr->GetOption<uint32>("Lottery.Lucky.TicketCostCount", 5));
    }

    bool TakeCurrency(Player* player, std::string const& type)
    {
        if (!player)
            return false;

        uint32 itemEntry = GetCurrencyItemEntry();
        uint32 cost = GetTicketCost(type);

        if (!itemEntry || cost == 0)
            return true;

        if (!player->HasItemCount(itemEntry, cost, true))
        {
            SendPlayer(player, Prefix() + " Voce precisa de item " + std::to_string(itemEntry) + " x" + std::to_string(cost) + " para comprar este bilhete.");
            return false;
        }

        player->DestroyItemCount(itemEntry, cost, true);
        return true;
    }

    uint32 CountPlayerTickets(uint32 roundId, Player* player)
    {
        if (!player)
            return 0;

        QueryResult result = CharacterDatabase.Query(
            "SELECT COUNT(*) FROM `zoecore_lottery_tickets` WHERE `round_id`={} AND `guid`={}",
            roundId, player->GetGUID().GetCounter());

        if (!result)
            return 0;

        return (*result)[0].Get<uint32>();
    }

    void StoreTicket(Player* player, uint32 roundId, std::string const& type, std::string const& numbers)
    {
        CharacterDatabase.Execute(
            "INSERT INTO `zoecore_lottery_tickets` (`round_id`, `type`, `guid`, `account_id`, `player_name`, `numbers`, `created_at`) VALUES ({}, '{}', {}, {}, '{}', '{}', {})",
            roundId, type, player->GetGUID().GetCounter(), player->GetSession()->GetAccountId(), player->GetName(), numbers, Now());
    }

    bool ValidateMegaNumbers(Player* player, std::vector<uint32> const& numbers)
    {
        uint32 minNumber = sConfigMgr->GetOption<uint32>("Lottery.Mega.MinNumber", 1);
        uint32 maxNumber = sConfigMgr->GetOption<uint32>("Lottery.Mega.MaxNumber", 60);
        uint32 pickCount = sConfigMgr->GetOption<uint32>("Lottery.Mega.PickCount", 6);

        if (numbers.size() != pickCount)
        {
            SendPlayer(player, Prefix() + " Digite exatamente " + std::to_string(pickCount) + " numeros.");
            return false;
        }

        for (uint32 n : numbers)
        {
            if (n < minNumber || n > maxNumber)
            {
                SendPlayer(player, Prefix() + " Numeros devem estar entre " + std::to_string(minNumber) + " e " + std::to_string(maxNumber) + ".");
                return false;
            }
        }

        return true;
    }

    bool ValidateLuckyNumber(Player* player, std::vector<uint32> const& numbers)
    {
        uint32 minNumber = sConfigMgr->GetOption<uint32>("Lottery.Lucky.MinNumber", 1);
        uint32 maxNumber = sConfigMgr->GetOption<uint32>("Lottery.Lucky.MaxNumber", 9999);

        if (numbers.size() != 1)
        {
            SendPlayer(player, Prefix() + " Digite apenas 1 numero.");
            return false;
        }

        if (numbers[0] < minNumber || numbers[0] > maxNumber)
        {
            SendPlayer(player, Prefix() + " Numero deve estar entre " + std::to_string(minNumber) + " e " + std::to_string(maxNumber) + ".");
            return false;
        }

        return true;
    }

    void BuyMegaTicket(Player* player, std::string manualCode = "")
    {
        if (!player || !sConfigMgr->GetOption<bool>("Lottery.Mega.Enable", true))
            return;

        uint32 roundId = EnsureActiveRound("MEGA");
        if (!roundId)
            return;

        uint32 maxTickets = sConfigMgr->GetOption<uint32>("Lottery.Mega.MaxTicketsPerPlayer", 10);
        if (maxTickets && CountPlayerTickets(roundId, player) >= maxTickets)
        {
            SendPlayer(player, Prefix() + " Voce ja atingiu o limite de bilhetes da Mega-Sena nesta rodada.");
            return;
        }

        std::string numbers;

        if (!manualCode.empty())
        {
            std::vector<uint32> parsed = ParseNumbers(manualCode);
            if (!ValidateMegaNumbers(player, parsed))
                return;

            numbers = JoinNumbers(parsed);
        }
        else
        {
            numbers = JoinNumbers(GenerateUniqueNumbers(
                sConfigMgr->GetOption<uint32>("Lottery.Mega.MinNumber", 1),
                sConfigMgr->GetOption<uint32>("Lottery.Mega.MaxNumber", 60),
                sConfigMgr->GetOption<uint32>("Lottery.Mega.PickCount", 6)));
        }

        uint32 cost = GetTicketCost("MEGA");
        if (!TakeCurrency(player, "MEGA"))
            return;

        StoreTicket(player, roundId, "MEGA", numbers);
        AddToJackpot("MEGA", cost);

        SendPlayer(player, Prefix() + " Bilhete Mega-Sena comprado! Seus numeros: |cff00ff00" + numbers + "|r");
    }

    void BuyLuckyTicket(Player* player, std::string manualCode = "")
    {
        if (!player || !sConfigMgr->GetOption<bool>("Lottery.Lucky.Enable", true))
            return;

        uint32 roundId = EnsureActiveRound("LUCKY");
        if (!roundId)
            return;

        uint32 maxTickets = sConfigMgr->GetOption<uint32>("Lottery.Lucky.MaxTicketsPerPlayer", 20);
        if (maxTickets && CountPlayerTickets(roundId, player) >= maxTickets)
        {
            SendPlayer(player, Prefix() + " Voce ja atingiu o limite de numeros da sorte nesta rodada.");
            return;
        }

        std::string numbers;

        if (!manualCode.empty())
        {
            std::vector<uint32> parsed = ParseNumbers(manualCode);
            if (!ValidateLuckyNumber(player, parsed))
                return;

            numbers = JoinNumbers(parsed);
        }
        else
        {
            uint32 minNumber = sConfigMgr->GetOption<uint32>("Lottery.Lucky.MinNumber", 1);
            uint32 maxNumber = sConfigMgr->GetOption<uint32>("Lottery.Lucky.MaxNumber", 9999);

            if (minNumber > maxNumber)
                std::swap(minNumber, maxNumber);

            numbers = std::to_string(urand(minNumber, maxNumber));
        }

        uint32 cost = GetTicketCost("LUCKY");
        if (!TakeCurrency(player, "LUCKY"))
            return;

        StoreTicket(player, roundId, "LUCKY", numbers);
        AddToJackpot("LUCKY", cost);

        SendPlayer(player, Prefix() + " Numero da Sorte comprado! Seu numero: |cff00ff00" + numbers + "|r");
    }

    uint32 InsertWinner(uint32 roundId, uint32 ticketId, std::string const& type, uint32 guid, std::string const& name, uint32 matchCount, std::vector<PrizeItem> const& prizes)
    {
        if (prizes.empty())
            return 0;

        uint32 firstItem = prizes[0].ItemEntry;
        uint32 totalCount = 0;
        for (PrizeItem const& p : prizes)
            totalCount += p.Count;

        CharacterDatabase.Execute(
            "INSERT INTO `zoecore_lottery_winners` (`round_id`, `ticket_id`, `type`, `guid`, `player_name`, `match_count`, `prize_item`, `prize_count`, `created_at`) VALUES ({}, {}, '{}', {}, '{}', {}, {}, {}, {})",
            roundId, ticketId, type, guid, name, matchCount, firstItem, totalCount, Now());

        QueryResult result = CharacterDatabase.Query("SELECT LAST_INSERT_ID()");
        if (!result)
            return 0;

        uint32 winnerId = (*result)[0].Get<uint32>();

        for (PrizeItem const& p : prizes)
        {
            if (!p.ItemEntry || !p.Count)
                continue;

            CharacterDatabase.Execute(
                "INSERT INTO `zoecore_lottery_winner_prizes` (`winner_id`, `round_id`, `guid`, `item_entry`, `item_count`, `source`) VALUES ({}, {}, {}, {}, {}, '{}')",
                winnerId, roundId, guid, p.ItemEntry, p.Count, p.Source);
        }

        return winnerId;
    }

    std::vector<TicketInfo> LoadMegaTickets(uint32 roundId, std::vector<uint32> const& drawn)
    {
        std::vector<TicketInfo> tickets;

        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `guid`, `player_name`, `numbers` FROM `zoecore_lottery_tickets` WHERE `round_id`={} AND `type`='MEGA'",
            roundId);

        if (!result)
            return tickets;

        do
        {
            Field* fields = result->Fetch();
            TicketInfo info;
            info.TicketId = fields[0].Get<uint32>();
            info.Guid = fields[1].Get<uint32>();
            info.PlayerName = fields[2].Get<std::string>();
            info.NumbersText = fields[3].Get<std::string>();
            info.Matches = CountMatches(ParseNumbers(info.NumbersText), drawn);
            tickets.push_back(info);

        } while (result->NextRow());

        return tickets;
    }

    void ProcessMegaRound(uint32 roundId, std::string const& drawnText)
    {
        std::vector<uint32> drawn = ParseNumbers(drawnText);
        std::vector<TicketInfo> tickets = LoadMegaTickets(roundId, drawn);

        uint32 match6Winners = 0;
        for (TicketInfo const& t : tickets)
            if (t.Matches >= 6)
                ++match6Winners;

        bool jackpotPaid = false;
        std::pair<uint32, uint32> jackpot = GetJackpot("MEGA");

        uint32 winners = 0;

        for (TicketInfo const& t : tickets)
        {
            std::vector<PrizeItem> prizes;

            if (t.Matches >= 6)
            {
                prizes = ConfigPrizeItems("Lottery.Mega.Prize.Match6.Items", "Lottery.Mega.Prize.Match6.ItemEntry", "Lottery.Mega.Prize.Match6.ItemCount", "match6");

                if (sConfigMgr->GetOption<bool>("Lottery.Mega.Jackpot.Enable", true) && jackpot.first && jackpot.second)
                {
                    uint32 jackpotCount = jackpot.second;
                    if (sConfigMgr->GetOption<bool>("Lottery.Mega.Jackpot.SplitBetweenWinners", true) && match6Winners > 1)
                        jackpotCount = jackpot.second / match6Winners;

                    if (jackpotCount)
                    {
                        prizes.push_back({ jackpot.first, jackpotCount, "jackpot" });
                        jackpotPaid = true;
                    }
                }
            }
            else if (t.Matches == 5)
                prizes = ConfigPrizeItems("Lottery.Mega.Prize.Match5.Items", "Lottery.Mega.Prize.Match5.ItemEntry", "Lottery.Mega.Prize.Match5.ItemCount", "match5");
            else if (t.Matches == 4)
                prizes = ConfigPrizeItems("Lottery.Mega.Prize.Match4.Items", "Lottery.Mega.Prize.Match4.ItemEntry", "Lottery.Mega.Prize.Match4.ItemCount", "match4");
            else if (t.Matches == 3)
                prizes = ParsePrizeItems(sConfigMgr->GetOption<std::string>("Lottery.Mega.Prize.Match3.Items", ""), "match3");

            if (!prizes.empty())
            {
                InsertWinner(roundId, t.TicketId, "MEGA", t.Guid, t.PlayerName, t.Matches, prizes);
                ++winners;
            }
        }

        if (jackpotPaid)
            ResetJackpot("MEGA");

        CharacterDatabase.Execute("UPDATE `zoecore_lottery_rounds` SET `winner_count`={} WHERE `id`={}", winners, roundId);
    }

    void ProcessLuckyRound(uint32 roundId, std::string const& drawnText)
    {
        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `guid`, `player_name`, `numbers` FROM `zoecore_lottery_tickets` WHERE `round_id`={} AND `type`='LUCKY'",
            roundId);

        if (!result)
        {
            CharacterDatabase.Execute("UPDATE `zoecore_lottery_rounds` SET `winner_count`=0 WHERE `id`={}", roundId);
            return;
        }

        std::vector<TicketInfo> winnersExact;

        do
        {
            Field* fields = result->Fetch();
            TicketInfo info;
            info.TicketId = fields[0].Get<uint32>();
            info.Guid = fields[1].Get<uint32>();
            info.PlayerName = fields[2].Get<std::string>();
            info.NumbersText = fields[3].Get<std::string>();
            info.Matches = info.NumbersText == drawnText ? 1 : 0;

            if (info.Matches)
                winnersExact.push_back(info);

        } while (result->NextRow());

        std::pair<uint32, uint32> jackpot = GetJackpot("LUCKY");
        bool jackpotPaid = false;

        for (TicketInfo const& t : winnersExact)
        {
            std::vector<PrizeItem> prizes = ConfigPrizeItems("Lottery.Lucky.Prize.Items", "Lottery.Lucky.Prize.ItemEntry", "Lottery.Lucky.Prize.ItemCount", "lucky");

            if (sConfigMgr->GetOption<bool>("Lottery.Lucky.Jackpot.Enable", true) && jackpot.first && jackpot.second)
            {
                uint32 jackpotCount = jackpot.second;
                if (sConfigMgr->GetOption<bool>("Lottery.Lucky.Jackpot.SplitBetweenWinners", true) && winnersExact.size() > 1)
                    jackpotCount = jackpot.second / uint32(winnersExact.size());

                if (jackpotCount)
                {
                    prizes.push_back({ jackpot.first, jackpotCount, "jackpot" });
                    jackpotPaid = true;
                }
            }

            InsertWinner(roundId, t.TicketId, "LUCKY", t.Guid, t.PlayerName, 1, prizes);
        }

        if (jackpotPaid)
            ResetJackpot("LUCKY");

        CharacterDatabase.Execute("UPDATE `zoecore_lottery_rounds` SET `winner_count`={} WHERE `id`={}", uint32(winnersExact.size()), roundId);
    }

    void DrawRound(std::string const& type, bool forced)
    {
        uint32 roundId = EnsureActiveRound(type);
        if (!roundId)
            return;

        QueryResult result = CharacterDatabase.Query(
            "SELECT `draw_at` FROM `zoecore_lottery_rounds` WHERE `id`={} AND `status`=0",
            roundId);

        if (!result)
            return;

        uint32 drawAt = (*result)[0].Get<uint32>();
        if (!forced && drawAt > Now())
            return;

        std::string drawnText;

        if (type == "MEGA")
        {
            drawnText = JoinNumbers(GenerateUniqueNumbers(
                sConfigMgr->GetOption<uint32>("Lottery.Mega.MinNumber", 1),
                sConfigMgr->GetOption<uint32>("Lottery.Mega.MaxNumber", 60),
                sConfigMgr->GetOption<uint32>("Lottery.Mega.PickCount", 6)));

            ProcessMegaRound(roundId, drawnText);
            Announce(Prefix() + " Resultado Mega-Sena: |cff00ff00" + drawnText + "|r. Use o NPC para resgatar premios.");
        }
        else
        {
            uint32 minNumber = sConfigMgr->GetOption<uint32>("Lottery.Lucky.MinNumber", 1);
            uint32 maxNumber = sConfigMgr->GetOption<uint32>("Lottery.Lucky.MaxNumber", 9999);

            if (minNumber > maxNumber)
                std::swap(minNumber, maxNumber);

            drawnText = std::to_string(urand(minNumber, maxNumber));
            ProcessLuckyRound(roundId, drawnText);
            Announce(Prefix() + " Resultado Numero da Sorte: |cff00ff00" + drawnText + "|r. Use o NPC para resgatar premios.");
        }

        CharacterDatabase.Execute(
            "UPDATE `zoecore_lottery_rounds` SET `status`=1, `drawn_at`={}, `drawn_numbers`='{}' WHERE `id`={}",
            Now(), drawnText, roundId);

        CreateRound(type);
    }

    void ClaimPrizes(Player* player)
    {
        if (!player)
            return;

        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `type`, `match_count` FROM `zoecore_lottery_winners` WHERE `guid`={} AND `claimed`=0 ORDER BY `id` ASC",
            player->GetGUID().GetCounter());

        if (!result)
        {
            SendPlayer(player, Prefix() + " Voce nao tem premios pendentes.");
            return;
        }

        uint32 claimed = 0;

        do
        {
            Field* fields = result->Fetch();
            uint32 winnerId = fields[0].Get<uint32>();
            std::string type = fields[1].Get<std::string>();
            uint32 matchCount = fields[2].Get<uint32>();

            QueryResult prizes = CharacterDatabase.Query(
                "SELECT `item_entry`, `item_count`, `source` FROM `zoecore_lottery_winner_prizes` WHERE `winner_id`={}",
                winnerId);

            if (!prizes)
                continue;

            bool delivered = true;

            do
            {
                Field* p = prizes->Fetch();
                uint32 item = p[0].Get<uint32>();
                uint32 count = p[1].Get<uint32>();
                std::string source = p[2].Get<std::string>();

                if (!item || !count)
                    continue;

                if (!player->AddItem(item, count))
                {
                    SendPlayer(player, Prefix() + " Espaco insuficiente na bag para resgatar todos os premios.");
                    delivered = false;
                    break;
                }

                SendPlayer(player, Prefix() + " Premio entregue: item " + std::to_string(item) + " x" + std::to_string(count) + " (" + source + ").");

            } while (prizes->NextRow());

            if (!delivered)
                break;

            CharacterDatabase.Execute("UPDATE `zoecore_lottery_winners` SET `claimed`=1, `claimed_at`={} WHERE `id`={}", Now(), winnerId);
            ++claimed;

            SendPlayer(player, Prefix() + " Premio resgatado: " + type + ", acertos " + std::to_string(matchCount) + ".");

        } while (result->NextRow());

        if (claimed)
            player->SaveToDB(false, true);
    }

    void SendLotteryInfo(Player* player)
    {
        if (!player)
            return;

        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `type`, `draw_at` FROM `zoecore_lottery_rounds` WHERE `status`=0 ORDER BY `type` ASC");

        if (!result)
        {
            SendPlayer(player, Prefix() + " Nenhum sorteio ativo no momento.");
            return;
        }

        SendPlayer(player, Prefix() + " Sorteios ativos:");

        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].Get<uint32>();
            std::string type = fields[1].Get<std::string>();
            uint32 drawAt = fields[2].Get<uint32>();
            uint32 remaining = drawAt > Now() ? drawAt - Now() : 0;
            std::pair<uint32, uint32> jackpot = GetJackpot(type);

            SendPlayer(player, "|cffFFFF00" + type + "|r rodada #" + std::to_string(id) +
                " - falta " + std::to_string(remaining / 60) + " min. Jackpot: item " +
                std::to_string(jackpot.first) + " x" + std::to_string(jackpot.second));

        } while (result->NextRow());
    }

    void SendMyTickets(Player* player)
    {
        if (!player)
            return;

        QueryResult result = CharacterDatabase.Query(
            "SELECT `type`, `round_id`, `numbers` FROM `zoecore_lottery_tickets` WHERE `guid`={} ORDER BY `id` DESC LIMIT 10",
            player->GetGUID().GetCounter());

        if (!result)
        {
            SendPlayer(player, Prefix() + " Voce ainda nao tem bilhetes.");
            return;
        }

        SendPlayer(player, Prefix() + " Seus ultimos bilhetes:");

        do
        {
            Field* fields = result->Fetch();
            std::string type = fields[0].Get<std::string>();
            uint32 roundId = fields[1].Get<uint32>();
            std::string numbers = fields[2].Get<std::string>();

            SendPlayer(player, type + " #" + std::to_string(roundId) + ": |cff00ff00" + numbers + "|r");

        } while (result->NextRow());
    }

    void SendHistory(Player* player)
    {
        if (!player)
            return;

        uint32 limit = sConfigMgr->GetOption<uint32>("Lottery.History.ShowLast", 10);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `type`, `drawn_numbers`, `winner_count`, `drawn_at` FROM `zoecore_lottery_rounds` WHERE `status`=1 ORDER BY `id` DESC LIMIT {}",
            limit);

        if (!result)
        {
            SendPlayer(player, Prefix() + " Ainda nao ha historico de sorteios.");
            return;
        }

        SendPlayer(player, Prefix() + " Ultimos sorteios:");

        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].Get<uint32>();
            std::string type = fields[1].Get<std::string>();
            std::string numbers = fields[2].Get<std::string>();
            uint32 winners = fields[3].Get<uint32>();

            SendPlayer(player, type + " #" + std::to_string(id) + ": |cff00ff00" + numbers + "|r - vencedores: " + std::to_string(winners));

        } while (result->NextRow());
    }

    void SendRanking(Player* player)
    {
        if (!player)
            return;

        uint32 limit = sConfigMgr->GetOption<uint32>("Lottery.Ranking.TopLimit", 10);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `player_name`, COUNT(*) AS wins, SUM(`match_count`) AS points FROM `zoecore_lottery_winners` GROUP BY `guid`, `player_name` ORDER BY wins DESC, points DESC LIMIT {}",
            limit);

        if (!result)
        {
            SendPlayer(player, Prefix() + " Ainda nao ha ganhadores no ranking.");
            return;
        }

        SendPlayer(player, Prefix() + " Ranking dos ganhadores:");

        uint32 pos = 1;
        do
        {
            Field* fields = result->Fetch();
            std::string name = fields[0].Get<std::string>();
            uint32 wins = fields[1].Get<uint32>();
            uint32 points = fields[2].Get<uint32>();

            SendPlayer(player, "#" + std::to_string(pos++) + " |cff00ff00" + name + "|r - vitorias: " + std::to_string(wins) + " - pontos: " + std::to_string(points));

        } while (result->NextRow());
    }

    void AddBackMenu(Player* player)
    {
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/Achievement_BG_returnXflags_def_WSG:30:30:-18:0|t Voltar", GOSSIP_SENDER_MAIN, ACTION_BACK);
    }

    void BuildActiveDrawsMenu(Player* player)
    {
        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_QuestionMark:30:30:-18:0|t Sorteios Ativos", GOSSIP_SENDER_MAIN, ACTION_BACK);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `type`, `draw_at` FROM `zoecore_lottery_rounds` WHERE `status`=0 ORDER BY `type` ASC, `id` ASC");

        if (!result)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nenhum sorteio ativo no momento.", GOSSIP_SENDER_MAIN, ACTION_BACK);
            AddBackMenu(player);
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].Get<uint32>();
            std::string type = fields[1].Get<std::string>();
            uint32 drawAt = fields[2].Get<uint32>();
            uint32 remaining = drawAt > Now() ? drawAt - Now() : 0;

            std::string icon = type == "MEGA" ? "INV_Misc_Coin_01" : "INV_Misc_Note_01";
            std::string prettyType = type == "MEGA" ? "Mega-Sena" : "Numero da Sorte";
            std::string line = "|TInterface/ICONS/" + icon + ":30:30:-18:0|t " + prettyType
                + " #" + std::to_string(id)
                + " - sorteio em " + std::to_string(remaining / 60) + " min";

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, line, GOSSIP_SENDER_MAIN, ACTION_BACK);

        } while (result->NextRow());

        AddBackMenu(player);
    }

    void BuildJackpotMenu(Player* player)
    {
        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_Coin_01:30:30:-18:0|t Jackpots Acumulados", GOSSIP_SENDER_MAIN, ACTION_BACK);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `type` FROM `zoecore_lottery_rounds` WHERE `status`=0 ORDER BY `type` ASC, `id` ASC");

        if (!result)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Nenhum jackpot ativo no momento.", GOSSIP_SENDER_MAIN, ACTION_BACK);
            AddBackMenu(player);
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].Get<uint32>();
            std::string type = fields[1].Get<std::string>();
            std::pair<uint32, uint32> jackpot = GetJackpot(type);

            std::string icon = type == "MEGA" ? "INV_Misc_Coin_01" : "INV_Misc_Note_01";
            std::string prettyType = type == "MEGA" ? "Mega-Sena" : "Numero da Sorte";
            std::string line = "|TInterface/ICONS/" + icon + ":30:30:-18:0|t " + prettyType
                + " #" + std::to_string(id)
                + " - Jackpot acumulado: " + std::to_string(jackpot.second) + "x premio";

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, line, GOSSIP_SENDER_MAIN, ACTION_BACK);

        } while (result->NextRow());

        AddBackMenu(player);
    }

    void BuildMyTicketsMenu(Player* player)
    {
        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_Ticket_Tarot_Blessings:30:30:-18:0|t Meus Bilhetes", GOSSIP_SENDER_MAIN, ACTION_BACK);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `type`, `round_id`, `numbers` FROM `zoecore_lottery_tickets` WHERE `guid`={} ORDER BY `id` DESC LIMIT 10",
            player->GetGUID().GetCounter());

        if (!result)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Voce ainda nao tem bilhetes.", GOSSIP_SENDER_MAIN, ACTION_BACK);
            AddBackMenu(player);
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            std::string type = fields[0].Get<std::string>();
            uint32 roundId = fields[1].Get<uint32>();
            std::string numbers = fields[2].Get<std::string>();

            std::string icon = type == "MEGA" ? "INV_Misc_Coin_02" : "INV_Misc_Note_02";
            std::string prettyType = type == "MEGA" ? "Mega-Sena" : "Numero da Sorte";
            std::string line = "|TInterface/ICONS/" + icon + ":30:30:-18:0|t " + prettyType + " #" + std::to_string(roundId)
                + " - Numeros: " + numbers;

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, line, GOSSIP_SENDER_MAIN, ACTION_BACK);

        } while (result->NextRow());

        AddBackMenu(player);
    }

    void BuildHistoryMenu(Player* player)
    {
        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Scroll_03:30:30:-18:0|t Historico de Sorteios", GOSSIP_SENDER_MAIN, ACTION_BACK);

        uint32 limit = sConfigMgr->GetOption<uint32>("Lottery.History.ShowLast", 10);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `type`, `drawn_numbers`, `winner_count`, `drawn_at` FROM `zoecore_lottery_rounds` WHERE `status`=1 ORDER BY `id` DESC LIMIT {}",
            limit);

        if (!result)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Ainda nao ha historico de sorteios.", GOSSIP_SENDER_MAIN, ACTION_BACK);
            AddBackMenu(player);
            return;
        }

        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].Get<uint32>();
            std::string type = fields[1].Get<std::string>();
            std::string numbers = fields[2].Get<std::string>();
            uint32 winners = fields[3].Get<uint32>();

            std::string icon = type == "MEGA" ? "INV_Misc_Coin_01" : "INV_Misc_Note_01";
            std::string prettyType = type == "MEGA" ? "Mega-Sena" : "Numero da Sorte";
            std::string line = "|TInterface/ICONS/" + icon + ":30:30:-18:0|t " + prettyType + " #" + std::to_string(id)
                + " - Numeros: " + numbers + " - Vencedores: " + std::to_string(winners);

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, line, GOSSIP_SENDER_MAIN, ACTION_BACK);

        } while (result->NextRow());

        AddBackMenu(player);
    }

    void BuildRankingMenu(Player* player)
    {
        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/Achievement_BG_winWSG:30:30:-18:0|t Ranking dos Ganhadores", GOSSIP_SENDER_MAIN, ACTION_BACK);

        uint32 limit = sConfigMgr->GetOption<uint32>("Lottery.Ranking.TopLimit", 10);

        QueryResult result = CharacterDatabase.Query(
            "SELECT `player_name`, COUNT(*) AS wins, SUM(`match_count`) AS points FROM `zoecore_lottery_winners` GROUP BY `guid`, `player_name` ORDER BY wins DESC, points DESC LIMIT {}",
            limit);

        if (!result)
        {
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Ainda nao ha ganhadores no ranking.", GOSSIP_SENDER_MAIN, ACTION_BACK);
            AddBackMenu(player);
            return;
        }

        uint32 pos = 1;
        do
        {
            Field* fields = result->Fetch();
            std::string name = fields[0].Get<std::string>();
            uint32 wins = fields[1].Get<uint32>();
            uint32 points = fields[2].Get<uint32>();

            std::string medal = pos == 1 ? "INV_Misc_Gem_Pearl_04" : (pos == 2 ? "INV_Misc_Gem_Pearl_03" : "INV_Misc_Gem_Pearl_02");
            std::string line = "|TInterface/ICONS/" + medal + ":30:30:-18:0|t #" + std::to_string(pos++)
                + " " + name + " - Vitorias: " + std::to_string(wins)
                + " - Pontos: " + std::to_string(points);

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, line, GOSSIP_SENDER_MAIN, ACTION_BACK);

        } while (result->NextRow());

        AddBackMenu(player);
    }

    void CheckBeforeDrawAnnouncements()
    {
        if (!sConfigMgr->GetOption<bool>("Lottery.Announce.BeforeDraw.Enable", true))
            return;

        std::vector<uint32> minutes = ParseMinuteList(sConfigMgr->GetOption<std::string>("Lottery.Announce.BeforeDraw.Minutes", "60,30,10,5,1"));
        if (minutes.empty())
            return;

        QueryResult result = CharacterDatabase.Query(
            "SELECT `id`, `type`, `draw_at` FROM `zoecore_lottery_rounds` WHERE `status`=0");

        if (!result)
            return;

        uint32 now = Now();

        do
        {
            Field* fields = result->Fetch();
            uint32 id = fields[0].Get<uint32>();
            std::string type = fields[1].Get<std::string>();
            uint32 drawAt = fields[2].Get<uint32>();

            if (drawAt <= now)
                continue;

            uint32 remainingSeconds = drawAt - now;
            uint32 remainingMinutes = (remainingSeconds + 59) / 60;

            for (uint32 threshold : minutes)
            {
                if (remainingMinutes > threshold)
                    continue;

                std::string key = type + "_" + std::to_string(id) + "_" + std::to_string(threshold);
                if (SentWarnings.find(key) != SentWarnings.end())
                    continue;

                SentWarnings.insert(key);
                Announce(Prefix() + " " + type + " sera sorteado em |cffFFFF00" + std::to_string(threshold) + " minuto(s)|r! Compre seu bilhete no NPC.");
                break;
            }

        } while (result->NextRow());
    }
}

class ZoeCoreLotteryNpc : public CreatureScript
{
public:
    ZoeCoreLotteryNpc() : CreatureScript("npc_zoecore_lottery") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature)
            return true;

        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Misc_Coin_01:30:30:-18:0|t Mega-Sena - bilhete automatico", GOSSIP_SENDER_MAIN, ACTION_BUY_MEGA_AUTO);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Misc_Coin_02:30:30:-18:0|t Mega-Sena - escolher numeros", GOSSIP_SENDER_MAIN, ACTION_BUY_MEGA_MANUAL, "Digite 6 numeros entre 1 e 60. Ex: 7 13 22 34 45 59", 0, true);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Misc_Note_01:30:30:-18:0|t Numero da Sorte - automatico", GOSSIP_SENDER_MAIN, ACTION_BUY_LUCKY_AUTO);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/INV_Misc_Note_02:30:30:-18:0|t Numero da Sorte - escolher numero", GOSSIP_SENDER_MAIN, ACTION_BUY_LUCKY_MANUAL, "Digite 1 numero da sorte.", 0, true);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_QuestionMark:30:30:-18:0|t Sorteios Ativos", GOSSIP_SENDER_MAIN, ACTION_ACTIVE_DRAWS);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_Coin_01:30:30:-18:0|t Jackpots Acumulados", GOSSIP_SENDER_MAIN, ACTION_JACKPOTS);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Misc_Ticket_Tarot_Blessings:30:30:-18:0|t Meus Bilhetes", GOSSIP_SENDER_MAIN, ACTION_MY_TICKETS);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/INV_Scroll_03:30:30:-18:0|t Historico de Sorteios", GOSSIP_SENDER_MAIN, ACTION_HISTORY);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/Achievement_BG_winWSG:30:30:-18:0|t Ranking dos Ganhadores", GOSSIP_SENDER_MAIN, ACTION_RANKING);
        AddGossipItemFor(player, GOSSIP_ICON_VENDOR, "|TInterface/ICONS/INV_Misc_Gem_Pearl_04:30:30:-18:0|t Resgatar Premios", GOSSIP_SENDER_MAIN, ACTION_CLAIM);

        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!player || !creature)
            return true;

        ClearGossipMenuFor(player);

        switch (action)
        {
            case ACTION_BUY_MEGA_AUTO:
                BuyMegaTicket(player);
                break;
            case ACTION_BUY_LUCKY_AUTO:
                BuyLuckyTicket(player);
                break;
            case ACTION_ACTIVE_DRAWS:
                BuildActiveDrawsMenu(player);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                return true;
            case ACTION_JACKPOTS:
                BuildJackpotMenu(player);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                return true;
            case ACTION_MY_TICKETS:
                BuildMyTicketsMenu(player);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                return true;
            case ACTION_HISTORY:
                BuildHistoryMenu(player);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                return true;
            case ACTION_RANKING:
                BuildRankingMenu(player);
                SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
                return true;
            case ACTION_CLAIM:
                ClaimPrizes(player);
                break;
            case ACTION_BACK:
                return OnGossipHello(player, creature);
            default:
                break;
        }

        CloseGossipMenuFor(player);
        return true;
    }

    bool OnGossipSelectCode(Player* player, Creature* creature, uint32 /*sender*/, uint32 action, char const* code) override
    {
        if (!player || !creature || !code)
            return true;

        ClearGossipMenuFor(player);

        switch (action)
        {
            case ACTION_BUY_MEGA_MANUAL:
                BuyMegaTicket(player, code);
                break;
            case ACTION_BUY_LUCKY_MANUAL:
                BuyLuckyTicket(player, code);
                break;
            default:
                break;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

class ZoeCoreLotteryWorldScript : public WorldScript
{
public:
    ZoeCoreLotteryWorldScript() : WorldScript("ZoeCoreLotteryWorldScript", { WORLDHOOK_ON_UPDATE }) { }

    void OnUpdate(uint32 diff) override
    {
        if (!LotteryEnabled() || !sConfigMgr->GetOption<bool>("Lottery.AutoDraw.Enable", true))
            return;

        if (diff >= WorldTickTimer)
        {
            WorldTickTimer = sConfigMgr->GetOption<uint32>("Lottery.AutoDraw.CheckIntervalSeconds", 30) * 1000;

            CheckBeforeDrawAnnouncements();

            if (sConfigMgr->GetOption<bool>("Lottery.Mega.Enable", true))
                DrawRound("MEGA", false);

            if (sConfigMgr->GetOption<bool>("Lottery.Lucky.Enable", true))
                DrawRound("LUCKY", false);

            return;
        }

        WorldTickTimer -= diff;
    }
};

using namespace Acore::ChatCommands;

class ZoeCoreLotteryCommandScript : public CommandScript
{
public:
    ZoeCoreLotteryCommandScript() : CommandScript("ZoeCoreLotteryCommandScript") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable LotteryCommandTable =
        {
            { "info", HandleInfoCommand, SEC_PLAYER, Console::Yes },
            { "drawmega", HandleDrawMegaCommand, SEC_GAMEMASTER, Console::Yes },
            { "drawlucky", HandleDrawLuckyCommand, SEC_GAMEMASTER, Console::Yes },
        };

        static ChatCommandTable BaseCommandTable =
        {
            { "lottery", LotteryCommandTable },
        };

        return BaseCommandTable;
    }

    static bool HandleInfoCommand(ChatHandler* handler)
    {
        if (!handler)
            return false;

        handler->SendSysMessage("ZoeCore Lottery V2: .lottery drawmega / .lottery drawlucky");
        return true;
    }

    static bool HandleDrawMegaCommand(ChatHandler* handler)
    {
        DrawRound("MEGA", true);

        if (handler)
            handler->SendSysMessage("Mega-Sena sorteada.");

        return true;
    }

    static bool HandleDrawLuckyCommand(ChatHandler* handler)
    {
        DrawRound("LUCKY", true);

        if (handler)
            handler->SendSysMessage("Numero da Sorte sorteado.");

        return true;
    }
};

void AddZoeCoreLotteryScripts()
{
    new ZoeCoreLotteryNpc();
    new ZoeCoreLotteryWorldScript();

    if (sConfigMgr->GetOption<bool>("Lottery.Commands.Enable", true))
        new ZoeCoreLotteryCommandScript();
}
