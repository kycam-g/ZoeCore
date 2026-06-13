/*
 * ZoeCore BG Reward V4
 * Anti-AFK / Activity Gate
 *
 * V4 Protected HK/Deaths Fix:
 * - na sua core GetHonorableKills() e GetDeaths() sao protected
 * - esta versão NÃO chama esses métodos
 * - participação é validada por Damage + Healing mínimo
 * - KillingBlows continua opcional porque GetKillingBlows() é público
 */

#include "Battleground.h"
#include "BattlegroundScore.h"
#include "Chat.h"
#include "Config.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "ScriptMgr.h"

#include <ctime>
#include <string>
#include <unordered_map>

namespace
{
    std::unordered_map<ObjectGuid::LowType, uint32> PlayerBgJoinTime;

    ObjectGuid::LowType Guid(Player* player)
    {
        return player ? player->GetGUID().GetCounter() : 0;
    }

    uint32 Now()
    {
        return uint32(std::time(nullptr));
    }

    void ReplaceAll(std::string& text, std::string const& search, std::string const& replace)
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

    std::string FormatMessage(std::string message, uint32 count, std::string const& currency)
    {
        ReplaceAll(message, "{count}", std::to_string(count));
        ReplaceAll(message, "{currency}", currency);
        ReplaceAll(message, "\\n", "\n");
        ReplaceAll(message, "{nl}", "\n");
        return message;
    }

    void SendPrivate(Player* player, std::string const& message)
    {
        if (!player || !player->GetSession())
            return;

        ChatHandler(player->GetSession()).SendSysMessage(message.c_str());
    }

    void SendBlocked(Player* player, std::string const& configKey, std::string const& fallback)
    {
        if (!sConfigMgr->GetOption<bool>("Battleground.Reward.Require.AnnounceBlocked", true))
            return;

        SendPrivate(player, sConfigMgr->GetOption<std::string>(configKey, fallback));
    }

    uint32 GetPlayerTimeInBgSeconds(Player* player, Battleground* bg)
    {
        if (!player)
            return 0;

        auto itr = PlayerBgJoinTime.find(Guid(player));
        if (itr != PlayerBgJoinTime.end())
        {
            uint32 now = Now();
            return now > itr->second ? now - itr->second : 0;
        }

        if (bg)
            return bg->GetStartTime() / 1000;

        return 0;
    }

    BattlegroundScore const* GetScore(Battleground* bg, Player* player)
    {
        if (!bg || !player)
            return nullptr;

        auto const* scores = bg->GetPlayerScores();
        if (!scores)
            return nullptr;

        auto itr = scores->find(player->GetGUID().GetCounter());
        if (itr == scores->end())
            return nullptr;

        return itr->second;
    }

    bool PassActivityRequirements(Battleground* bg, Player* player)
    {
        if (!sConfigMgr->GetOption<bool>("Battleground.Reward.Require.Activity.Enable", true))
            return true;

        if (!bg || !player)
            return false;

        if (sConfigMgr->GetOption<bool>("Battleground.Reward.Require.NotAFK", true) && player->isAFK())
        {
            SendBlocked(player, "Battleground.Reward.Message.Blocked.AFK",
                "|cffff2020Recompensa bloqueada:|r voce terminou a BG marcado como AFK.");
            return false;
        }

        uint32 minTime = sConfigMgr->GetOption<uint32>("Battleground.Reward.Require.MinTimeSeconds", 120);
        if (minTime > 0 && GetPlayerTimeInBgSeconds(player, bg) < minTime)
        {
            SendBlocked(player, "Battleground.Reward.Message.Blocked.Time",
                "|cffff2020Recompensa bloqueada:|r voce ficou pouco tempo na BG.");
            return false;
        }

        BattlegroundScore const* score = GetScore(bg, player);
        if (!score)
        {
            if (sConfigMgr->GetOption<bool>("Battleground.Reward.Require.BlockIfNoScore", true))
            {
                SendBlocked(player, "Battleground.Reward.Message.Blocked.NoScore",
                    "|cffff2020Recompensa bloqueada:|r nao foi possivel validar sua participacao na BG.");
                return false;
            }

            return true;
        }

        uint32 minDamageHealing = sConfigMgr->GetOption<uint32>("Battleground.Reward.Require.MinDamageOrHealing", 10000);
        if (minDamageHealing > 0)
        {
            uint32 activityValue = score->GetDamageDone() + score->GetHealingDone();
            if (activityValue < minDamageHealing)
            {
                SendBlocked(player, "Battleground.Reward.Message.Blocked.DamageHealing",
                    "|cffff2020Recompensa bloqueada:|r voce nao causou dano/cura suficiente.");
                return false;
            }
        }

        // V4: não usa GetHonorableKills() nem GetDeaths(), pois na sua core eles são protected.
        // Para validar "participação", usamos dano+cura mínimo e, opcionalmente, killing blows.
        uint32 minKillingBlows = sConfigMgr->GetOption<uint32>("Battleground.Reward.Require.MinKillingBlows", 0);
        if (minKillingBlows > 0 && score->GetKillingBlows() < minKillingBlows)
        {
            SendBlocked(player, "Battleground.Reward.Message.Blocked.Participation",
                "|cffff2020Recompensa bloqueada:|r voce nao atingiu os requisitos de participacao da BG.");
            return false;
        }

        return true;
    }

    uint32 GetBattlegroundRewardItem(Player* player)
    {
        if (!player)
            return sConfigMgr->GetOption<uint32>("Battleground.Reward.ItemID.Default", 900001);

        switch (player->GetZoneId())
        {
            case 3277:
                return sConfigMgr->GetOption<uint32>("Battleground.Reward.ItemID.WS", 900001);
            case 3358:
                return sConfigMgr->GetOption<uint32>("Battleground.Reward.ItemID.AB", 900001);
            case 3820:
                return sConfigMgr->GetOption<uint32>("Battleground.Reward.ItemID.EY", 900001);
            case 4710:
                return sConfigMgr->GetOption<uint32>("Battleground.Reward.ItemID.IC", 900001);
            case 4384:
                return sConfigMgr->GetOption<uint32>("Battleground.Reward.ItemID.SA", 900001);
            case 2597:
                return sConfigMgr->GetOption<uint32>("Battleground.Reward.ItemID.AV", 900001);
            default:
                return sConfigMgr->GetOption<uint32>("Battleground.Reward.ItemID.Default", 900001);
        }
    }

    void RewardBattleground(Battleground* bg, Player* player, bool winner)
    {
        if (!bg || !player)
            return;

        if (!PassActivityRequirements(bg, player))
        {
            PlayerBgJoinTime.erase(Guid(player));
            return;
        }

        uint32 count = winner
            ? sConfigMgr->GetOption<uint32>("Battleground.Reward.WinnerTeam.Count", 8)
            : sConfigMgr->GetOption<uint32>("Battleground.Reward.LoserTeam.Count", 3);

        if (count == 0)
        {
            PlayerBgJoinTime.erase(Guid(player));
            return;
        }

        uint32 itemId = GetBattlegroundRewardItem(player);
        if (itemId == 0)
        {
            PlayerBgJoinTime.erase(Guid(player));
            return;
        }

        player->AddItem(itemId, count);

        if (sConfigMgr->GetOption<bool>("Battleground.Reward.Announce.Private", true))
        {
            std::string currency = sConfigMgr->GetOption<std::string>("Battleground.Reward.CurrencyName", "Fragmento Cruel");
            std::string msg = winner
                ? sConfigMgr->GetOption<std::string>("Battleground.Reward.Message.Winner", "|cff00ff00Vitoria na BG!|r Voce recebeu |cffffff00{count}x {currency}|r.")
                : sConfigMgr->GetOption<std::string>("Battleground.Reward.Message.Loser", "|cffffcc00Fim da BG!|r Voce recebeu |cffffff00{count}x {currency}|r pela participacao.");

            SendPrivate(player, FormatMessage(msg, count, currency));
        }

        PlayerBgJoinTime.erase(Guid(player));
    }

    uint32 GetArenaRewardItem(Battleground* bg, bool winner)
    {
        if (!bg)
            return 0;

        uint32 arenaType = bg->GetArenaType();
        uint32 arenaType1v1 = sConfigMgr->GetOption<uint32>("Arena.Reward.1v1.ArenaType", 1);
        uint32 arenaType3v3Solo = sConfigMgr->GetOption<uint32>("Arena.Reward.3v3soloQ.ArenaType", 4);

        if (arenaType == arenaType1v1)
            return winner ? sConfigMgr->GetOption<uint32>("Arena.Reward.Winner.ItemID.1v1", 900001) : sConfigMgr->GetOption<uint32>("Arena.Reward.Loser.ItemID.1v1", 900001);

        if (arenaType == arenaType3v3Solo)
            return winner ? sConfigMgr->GetOption<uint32>("Arena.Reward.Winner.ItemID.3v3Solo", 900001) : sConfigMgr->GetOption<uint32>("Arena.Reward.Loser.ItemID.3v3Solo", 900001);

        switch (arenaType)
        {
            case ARENA_TEAM_2v2:
                return winner ? sConfigMgr->GetOption<uint32>("Arena.Reward.Winner.ItemID.2v2", 900001) : sConfigMgr->GetOption<uint32>("Arena.Reward.Loser.ItemID.2v2", 900001);
            case ARENA_TEAM_3v3:
                return winner ? sConfigMgr->GetOption<uint32>("Arena.Reward.Winner.ItemID.3v3", 900001) : sConfigMgr->GetOption<uint32>("Arena.Reward.Loser.ItemID.3v3", 900001);
            case ARENA_TEAM_5v5:
                return winner ? sConfigMgr->GetOption<uint32>("Arena.Reward.Winner.ItemID.5v5", 900001) : sConfigMgr->GetOption<uint32>("Arena.Reward.Loser.ItemID.5v5", 900001);
            default:
                return 0;
        }
    }

    void RewardArena(Battleground* bg, Player* player, bool winner)
    {
        if (!bg || !player)
            return;

        uint32 count = winner
            ? sConfigMgr->GetOption<uint32>("Arena.Reward.WinnerTeam.Count", 2)
            : sConfigMgr->GetOption<uint32>("Arena.Reward.LoserTeam.Count", 1);

        if (count == 0)
            return;

        uint32 itemId = GetArenaRewardItem(bg, winner);
        if (itemId == 0)
            return;

        player->AddItem(itemId, count);

        if (sConfigMgr->GetOption<bool>("Arena.Reward.Announce.Private", true))
        {
            std::string currency = sConfigMgr->GetOption<std::string>("Arena.Reward.CurrencyName", "Fragmento Cruel");
            std::string msg = winner
                ? sConfigMgr->GetOption<std::string>("Arena.Reward.Message.Winner", "|cff00ff00Vitoria na Arena!|r Voce recebeu |cffffff00{count}x {currency}|r.")
                : sConfigMgr->GetOption<std::string>("Arena.Reward.Message.Loser", "|cffffcc00Fim da Arena!|r Voce recebeu |cffffff00{count}x {currency}|r pela participacao.");

            SendPrivate(player, FormatMessage(msg, count, currency));
        }
    }
}

class BGReward_PlayerScript : public PlayerScript
{
public:
    BGReward_PlayerScript() : PlayerScript("BGReward_PlayerScript", {
        PLAYERHOOK_ON_MAP_CHANGED,
        PLAYERHOOK_ON_LOGOUT
    }) { }

    void OnPlayerMapChanged(Player* player) override
    {
        if (!player)
            return;

        auto guid = Guid(player);

        if (player->InBattleground() && !player->InArena() && player->GetBattleground())
        {
            PlayerBgJoinTime[guid] = Now();
            return;
        }

        PlayerBgJoinTime.erase(guid);
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!player)
            return;

        PlayerBgJoinTime.erase(Guid(player));
    }
};

class BGScript_BattlegroundsReward : public BGScript
{
public:
    BGScript_BattlegroundsReward() : BGScript("BGScript_BattlegroundsReward", {
        ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_END_REWARD
    }) { }

    void OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId winnerTeamId) override
    {
        if (!bg || !player)
            return;

        bool winner = player->GetBgTeamId() == winnerTeamId;

        if (!bg->isArena())
        {
            if (sConfigMgr->GetOption<bool>("Battleground.Reward.Enable", true))
                RewardBattleground(bg, player, winner);

            return;
        }

        if (sConfigMgr->GetOption<bool>("Arena.Reward.Enable", false) && bg->isRated())
            RewardArena(bg, player, winner);
    }
};

void AddSC_BattlegroundsReward()
{
    new BGReward_PlayerScript();
    new BGScript_BattlegroundsReward();
}
