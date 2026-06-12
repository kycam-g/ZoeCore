/*
 * ZoeCore KillStreak
 *
 * PvP kill streak system for AzerothCore WotLK.
 * Features:
 * - First Blood
 * - 5/10/15/20/30 kill streaks
 * - Marker aura
 * - Optional power buffs
 * - Rewards and shutdown bounty
 * - Anti-farm
 * - BG/Arena/PvP Zone/World scope
 */

#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Chat.h"
#include "Log.h"
#include "World.h"
#include "WorldPacket.h"
#include "WorldSession.h"
#include "Opcodes.h"
#include "Map.h"
#include "Battleground.h"

#include <algorithm>
#include <ctime>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{
    void SendPlaySoundToPlayer(Player* player, uint32 soundId);
    void PlaySoundToMap(Player* source, uint32 soundId);
    void PlayStreakSound(Player* source);
    void PlayShutdownSound(Player* source);

    void ApplyKillStreakScale(Player* player);
    void ResetKillStreakScale(Player* player);
    bool ShouldRemoveMarkerForReason(char const* reason);
    bool ShouldResetScaleForReason(char const* reason);
    void ResetVisualState(Player* player, char const* reason);

    struct StreakInfo
    {
        uint32 Count = 0;
        uint32 LastKillTime = 0;
    };

    std::unordered_map<uint32, StreakInfo> PlayerStreaks;
    std::unordered_map<uint64, uint32> LastVictimKillTime;
    std::unordered_map<uint64, bool> FirstBloodDone;

    uint32 Now()
    {
        return uint32(std::time(nullptr));
    }

    bool Enabled()
    {
        return sConfigMgr->GetOption<bool>("ZoeKillStreak.Enable", true);
    }

    std::string Prefix()
    {
        return sConfigMgr->GetOption<std::string>("ZoeKillStreak.MessagePrefix", "[ZoeCore PvP]");
    }

    uint32 Guid(Player* player)
    {
        return player ? player->GetGUID().GetCounter() : 0;
    }

    uint64 MakePairKey(uint32 a, uint32 b)
    {
        return (uint64(a) << 32) | uint64(b);
    }

    uint64 GetContextKey(Player* player)
    {
        if (!player)
            return 0;

        uint32 mapId = player->GetMapId();
        uint32 instanceId = player->GetMap() ? player->GetMap()->GetInstanceId() : 0;

        return (uint64(mapId) << 32) | uint64(instanceId);
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

    std::vector<uint32> TokenizeUInt32List(std::string const& value)
    {
        std::vector<uint32> result;
        std::stringstream ss(value);
        std::string token;

        while (std::getline(ss, token, ';'))
        {
            if (token.empty())
                continue;

            try
            {
                result.push_back(uint32(std::stoul(token)));
            }
            catch (...)
            {
            }
        }

        return result;
    }

    bool IsInList(uint32 value, std::vector<uint32> const& list)
    {
        return std::find(list.begin(), list.end(), value) != list.end();
    }

    bool IsPvPZone(Player* player)
    {
        if (!player)
            return false;

        std::vector<uint32> zoneIds = TokenizeUInt32List(sConfigMgr->GetOption<std::string>("ZoeKillStreak.Scope.PvPZoneIds", ""));
        std::vector<uint32> areaIds = TokenizeUInt32List(sConfigMgr->GetOption<std::string>("ZoeKillStreak.Scope.PvPAreaIds", ""));

        if (!zoneIds.empty() && IsInList(player->GetZoneId(), zoneIds))
            return true;

        if (!areaIds.empty() && IsInList(player->GetAreaId(), areaIds))
            return true;

        return player->IsPvP();
    }

    bool IsAllowedScope(Player* player)
    {
        if (!player)
            return false;

        if (player->InArena())
            return sConfigMgr->GetOption<bool>("ZoeKillStreak.Scope.Arena", true);

        if (player->InBattleground())
            return sConfigMgr->GetOption<bool>("ZoeKillStreak.Scope.Battleground", true);

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Scope.PvPZones", true) && IsPvPZone(player))
            return true;

        return sConfigMgr->GetOption<bool>("ZoeKillStreak.Scope.World", false);
    }

    bool IsSameAccount(Player* killer, Player* victim)
    {
        if (!killer || !victim || !killer->GetSession() || !victim->GetSession())
            return false;

        return killer->GetSession()->GetAccountId() == victim->GetSession()->GetAccountId();
    }

    bool AntiFarmAllows(Player* killer, Player* victim)
    {
        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.AntiFarm.Enable", true))
            return true;

        if (!killer || !victim)
            return false;

        uint32 minLevel = sConfigMgr->GetOption<uint32>("ZoeKillStreak.AntiFarm.MinLevel", 80);
        if (killer->GetLevel() < minLevel || victim->GetLevel() < minLevel)
            return false;

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.AntiFarm.BlockSameAccount", true) && IsSameAccount(killer, victim))
            return false;

        uint32 cooldown = sConfigMgr->GetOption<uint32>("ZoeKillStreak.AntiFarm.SameVictimCooldownSeconds", 300);
        if (cooldown > 0)
        {
            uint64 key = MakePairKey(Guid(killer), Guid(victim));
            uint32 now = Now();

            auto itr = LastVictimKillTime.find(key);
            if (itr != LastVictimKillTime.end() && now < itr->second + cooldown)
                return false;

            LastVictimKillTime[key] = now;
        }

        return true;
    }

    uint32 HighestBracket(uint32 streak)
    {
        if (streak >= 30)
            return 30;

        if (streak >= 20)
            return 20;

        if (streak >= 15)
            return 15;

        if (streak >= 10)
            return 10;

        if (streak >= 5)
            return 5;

        return 0;
    }

    bool IsMilestone(uint32 streak)
    {
        return streak == 5 || streak == 10 || streak == 15 || streak == 20 || streak == 30;
    }

    uint32 ConfigUInt(std::string const& key, uint32 defaultValue)
    {
        return sConfigMgr->GetOption<uint32>(key, defaultValue);
    }

    std::string ConfigString(std::string const& key, std::string const& defaultValue)
    {
        return sConfigMgr->GetOption<std::string>(key, defaultValue);
    }

    void SendPrivate(Player* player, std::string const& message)
    {
        if (!player || !player->GetSession())
            return;

        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Announce.Private", true))
            return;

        ChatHandler(player->GetSession()).PSendSysMessage("{} {}", Prefix().c_str(), message.c_str());
    }

    void SendPlaySoundToPlayer(Player* player, uint32 soundId)
    {
        if (!player || !player->GetSession() || soundId == 0)
            return;

        WorldPacket data(SMSG_PLAY_SOUND, 4);
        data << uint32(soundId);
        player->GetSession()->SendPacket(&data);
    }

    void PlaySoundToMap(Player* source, uint32 soundId)
    {
        if (!source || !source->GetMap() || soundId == 0)
            return;

        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            if (Player* player = itr->GetSource())
                SendPlaySoundToPlayer(player, soundId);
        }
    }

    void PlayStreakSound(Player* source)
    {
        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Sound.Enable", true))
            return;

        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Sound.Streak.Enable", true))
            return;

        uint32 soundId = sConfigMgr->GetOption<uint32>("ZoeKillStreak.Sound.Streak.SoundId", 8232);
        PlaySoundToMap(source, soundId);
    }

    void PlayShutdownSound(Player* source)
    {
        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Sound.Enable", true))
            return;

        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Sound.Shutdown.Enable", true))
            return;

        uint32 soundId = sConfigMgr->GetOption<uint32>("ZoeKillStreak.Sound.Shutdown.SoundId", 8960);
        PlaySoundToMap(source, soundId);
    }

    void SendTopScreenToMap(Player* source, std::string const& message)
    {
        if (!source || !source->GetMap())
            return;

        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Announce.TopScreen", true))
            return;

        Map::PlayerList const& players = source->GetMap()->GetPlayers();
        for (Map::PlayerList::const_iterator itr = players.begin(); itr != players.end(); ++itr)
        {
            if (Player* player = itr->GetSource())
            {
                if (player->GetSession())
                    player->GetSession()->SendAreaTriggerMessage(message.c_str());
            }
        }
    }

    void SendWorldChat(std::string const& message)
    {
        // Compat fix:
        // Em algumas revisões do AzerothCore, sWorld é IWorld e não possui SendServerMessage.
        // Para evitar erro de compilação, o V2 deixa o chat global como no-op.
        // O anúncio principal continua funcionando via top-screen no mapa/BG/Arena.
        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Announce.WorldChat", false))
            return;

        (void)message;

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Log.Enable", true))
            LOG_INFO("module", "ZoeCore KillStreak: WorldChat está ativado na config, mas foi ignorado nesta build para compatibilidade com IWorld.");
    }

    void Announce(Player* source, std::string message)
    {
        if (!source)
            return;

        SendTopScreenToMap(source, message);
        SendWorldChat(message);
    }

    std::string FormatMessage(std::string message, Player* killer, Player* victim, uint32 streak, std::string const& rewardText)
    {
        ReplaceAll(message, "{killer}", killer ? killer->GetName() : "Desconhecido");
        ReplaceAll(message, "{victim}", victim ? victim->GetName() : "Desconhecido");
        ReplaceAll(message, "{streak}", std::to_string(streak));
        ReplaceAll(message, "{reward}", rewardText);

        // ZoeCore V5:
        // Permite quebrar linha pela config usando \\n ou {nl}.
        // Exemplo no .conf: "Linha 1\\nLinha 2"
        ReplaceAll(message, "\\n", "\n");
        ReplaceAll(message, "{nl}", "\n");

        return message;
    }

    std::string BuildRewardText(uint32 honor, uint32 itemCount, uint32 itemEntry)
    {
        std::string reward;

        if (honor > 0)
            reward += std::to_string(honor) + " Honor";

        if (itemCount > 0)
        {
            if (!reward.empty())
                reward += " + ";

            reward += std::to_string(itemCount) + "x item " + std::to_string(itemEntry);
        }

        if (reward.empty())
            reward = "sem recompensa";

        return reward;
    }

    void GiveReward(Player* player, uint32 honor, uint32 itemEntry, uint32 itemCount)
    {
        if (!player)
            return;

        if (honor > 0)
            player->ModifyHonorPoints(int32(honor));

        if (itemEntry > 0 && itemCount > 0)
            player->AddItem(itemEntry, itemCount);
    }

    void ApplyKillStreakScale(Player* player)
    {
        if (!player)
            return;

        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Scale.Enable", true))
            return;

        float scale = sConfigMgr->GetOption<float>("ZoeKillStreak.Scale.Value", 1.15f);
        if (scale <= 0.0f)
            scale = 1.0f;

        player->SetObjectScale(scale);
    }

    void ResetKillStreakScale(Player* player)
    {
        if (!player)
            return;

        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Scale.Enable", true))
            return;

        float scale = sConfigMgr->GetOption<float>("ZoeKillStreak.Scale.ResetValue", 1.0f);
        if (scale <= 0.0f)
            scale = 1.0f;

        player->SetObjectScale(scale);
    }

    void RemoveMarkerAndPower(Player* player)
    {
        ResetVisualState(player, "default");
    }

    uint32 PowerBuffForStreak(uint32 streak)
    {
        switch (HighestBracket(streak))
        {
            case 5: return sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.5.SpellId", 0);
            case 10: return sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.10.SpellId", 0);
            case 15: return sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.15.SpellId", 0);
            case 20: return sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.20.SpellId", 0);
            case 30: return sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.30.SpellId", 0);
            default: return 0;
        }
    }

    void ApplyMarkerAndPower(Player* player, uint32 streak)
    {
        if (!player || streak < 5)
            return;

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Aura.Marker.Enable", true))
        {
            uint32 marker = sConfigMgr->GetOption<uint32>("ZoeKillStreak.Aura.Marker.SpellId", 42171);
            if (marker > 0 && !player->HasAura(marker))
                player->AddAura(marker, player);

            ApplyKillStreakScale(player);
        }

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.PowerBuff.Enable", false))
        {
            RemoveMarkerAndPower(player);

            uint32 marker = sConfigMgr->GetOption<uint32>("ZoeKillStreak.Aura.Marker.SpellId", 42171);
            if (marker > 0 && sConfigMgr->GetOption<bool>("ZoeKillStreak.Aura.Marker.Enable", true))
                player->AddAura(marker, player);

            ApplyKillStreakScale(player);

            uint32 buff = PowerBuffForStreak(streak);
            if (buff > 0)
                player->AddAura(buff, player);
        }
    }

    bool ShouldRemoveMarkerForReason(char const* reason)
    {
        std::string r = reason ? reason : "";

        if (r == "death")
            return sConfigMgr->GetOption<bool>("ZoeKillStreak.Aura.Marker.RemoveOnDeath", true);

        if (r == "logout")
            return sConfigMgr->GetOption<bool>("ZoeKillStreak.Aura.Marker.RemoveOnLogout", true);

        if (r == "mapchange")
            return sConfigMgr->GetOption<bool>("ZoeKillStreak.Aura.Marker.RemoveOnMapChange", true);

        return true;
    }

    bool ShouldResetScaleForReason(char const* reason)
    {
        std::string r = reason ? reason : "";

        if (r == "death")
            return sConfigMgr->GetOption<bool>("ZoeKillStreak.Scale.Reset.OnDeath", true);

        if (r == "logout")
            return sConfigMgr->GetOption<bool>("ZoeKillStreak.Scale.Reset.OnLogout", true);

        if (r == "mapchange")
            return sConfigMgr->GetOption<bool>("ZoeKillStreak.Scale.Reset.OnMapChange", true);

        return true;
    }

    void ResetVisualState(Player* player, char const* reason)
    {
        if (!player)
            return;

        if (ShouldResetScaleForReason(reason))
            ResetKillStreakScale(player);

        if (ShouldRemoveMarkerForReason(reason))
        {
            uint32 marker = sConfigMgr->GetOption<uint32>("ZoeKillStreak.Aura.Marker.SpellId", 42171);
            if (marker > 0)
                player->RemoveAura(marker);
        }

        uint32 buffs[] =
        {
            sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.5.SpellId", 0),
            sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.10.SpellId", 0),
            sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.15.SpellId", 0),
            sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.20.SpellId", 0),
            sConfigMgr->GetOption<uint32>("ZoeKillStreak.PowerBuff.30.SpellId", 0)
        };

        for (uint32 spellId : buffs)
        {
            if (spellId > 0)
                player->RemoveAura(spellId);
        }
    }

    void ResetStreak(Player* player, char const* reason = "default")
    {
        if (!player)
            return;

        PlayerStreaks.erase(Guid(player));
        ResetVisualState(player, reason);
    }

    void RewardMilestone(Player* killer, Player* victim, uint32 streak)
    {
        if (!killer || !sConfigMgr->GetOption<bool>("ZoeKillStreak.Reward.Enable", true))
            return;

        uint32 itemEntry = sConfigMgr->GetOption<uint32>("ZoeKillStreak.Reward.ItemEntry", 900001);
        uint32 itemCount = ConfigUInt("ZoeKillStreak.Reward." + std::to_string(streak) + ".ItemCount", 0);
        uint32 honor = ConfigUInt("ZoeKillStreak.Reward." + std::to_string(streak) + ".Honor", 0);

        GiveReward(killer, honor, itemEntry, itemCount);

        std::string rewardText = BuildRewardText(honor, itemCount, itemEntry);
        std::string key = "ZoeKillStreak.Message." + std::to_string(streak);
        std::string msg = FormatMessage(ConfigString(key, "{killer} esta em kill streak de {streak}!"), killer, victim, streak, rewardText);

        Announce(killer, msg);
        PlayStreakSound(killer);
        SendPrivate(killer, "|cff00ff00Bonus de streak recebido:|r " + rewardText);

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Log.Enable", true))
        {
            LOG_INFO("module", "ZoeCore KillStreak: milestone killer='{}' guid={} streak={} honor={} item={} count={}",
                killer->GetName(), Guid(killer), streak, honor, itemEntry, itemCount);
        }
    }

    void RewardShutdown(Player* killer, Player* victim, uint32 victimStreak)
    {
        if (!killer || !victim)
            return;

        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.Bounty.Enable", true))
            return;

        uint32 minStreak = sConfigMgr->GetOption<uint32>("ZoeKillStreak.Bounty.MinStreak", 5);
        if (victimStreak < minStreak)
            return;

        uint32 bracket = HighestBracket(victimStreak);
        if (bracket == 0)
            return;

        uint32 itemEntry = sConfigMgr->GetOption<uint32>("ZoeKillStreak.Bounty.ItemEntry", 900001);
        uint32 itemCount = ConfigUInt("ZoeKillStreak.Bounty." + std::to_string(bracket) + ".ItemCount", 0);
        uint32 honor = ConfigUInt("ZoeKillStreak.Bounty." + std::to_string(bracket) + ".Honor", 0);

        GiveReward(killer, honor, itemEntry, itemCount);

        std::string rewardText = BuildRewardText(honor, itemCount, itemEntry);
        std::string msg = FormatMessage(ConfigString("ZoeKillStreak.Message.Shutdown", "{killer} encerrou a sequencia de {victim}!"), killer, victim, victimStreak, rewardText);
        Announce(killer, msg);
        PlayShutdownSound(killer);

        SendPrivate(killer, "|cffffcc00Bonus por shutdown:|r " + rewardText);

        std::string victimMsg = FormatMessage(ConfigString("ZoeKillStreak.Message.VictimLost", "Sua sequencia de {streak} kills acabou!"), killer, victim, victimStreak, rewardText);
        SendPrivate(victim, victimMsg);

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Log.Enable", true))
        {
            LOG_INFO("module", "ZoeCore KillStreak: shutdown killer='{}' victim='{}' victimStreak={} honor={} item={} count={}",
                killer->GetName(), victim->GetName(), victimStreak, honor, itemEntry, itemCount);
        }
    }

    void HandleFirstBlood(Player* killer, Player* victim)
    {
        if (!killer || !victim)
            return;

        if (!sConfigMgr->GetOption<bool>("ZoeKillStreak.FirstBlood.Enable", true))
            return;

        uint64 context = GetContextKey(killer);
        if (FirstBloodDone[context])
            return;

        FirstBloodDone[context] = true;

        std::string msg = FormatMessage(ConfigString("ZoeKillStreak.Message.FirstBlood", "FIRST BLOOD! {killer} matou {victim}!"), killer, victim, 1, "");
        Announce(killer, msg);

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Log.Enable", true))
        {
            LOG_INFO("module", "ZoeCore KillStreak: first blood killer='{}' victim='{}' context={}",
                killer->GetName(), victim->GetName(), context);
        }
    }

    void AddKill(Player* killer, Player* victim)
    {
        uint32 killerGuid = Guid(killer);

        StreakInfo& info = PlayerStreaks[killerGuid];
        info.Count += 1;
        info.LastKillTime = Now();

        ApplyMarkerAndPower(killer, info.Count);

        if (IsMilestone(info.Count))
            RewardMilestone(killer, victim, info.Count);
    }
}

class ZoeCoreKillStreak : public PlayerScript
{
public:
    ZoeCoreKillStreak() : PlayerScript("ZoeCoreKillStreak", {
        PLAYERHOOK_ON_PVP_KILL,
        PLAYERHOOK_ON_PLAYER_JUST_DIED,
        PLAYERHOOK_ON_LOGOUT,
        PLAYERHOOK_ON_MAP_CHANGED
    }) { }

    void OnPlayerPVPKill(Player* killer, Player* killed) override
    {
        if (!Enabled())
            return;

        if (!killer || !killed || killer == killed)
            return;

        if (!IsAllowedScope(killer))
            return;

        uint32 victimStreak = 0;
        auto victimItr = PlayerStreaks.find(Guid(killed));
        if (victimItr != PlayerStreaks.end())
            victimStreak = victimItr->second.Count;

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Reset.OnDeath", true))
            ResetStreak(killed, "death");

        if (!AntiFarmAllows(killer, killed))
        {
            if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Log.Enable", true))
            {
                LOG_INFO("module", "ZoeCore KillStreak: anti-farm blocked killer='{}' victim='{}'",
                    killer->GetName(), killed->GetName());
            }

            return;
        }

        HandleFirstBlood(killer, killed);
        RewardShutdown(killer, killed, victimStreak);
        AddKill(killer, killed);
    }

    void OnPlayerJustDied(Player* player) override
    {
        if (!Enabled())
            return;

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Reset.OnDeath", true))
            ResetStreak(player, "death");
    }

    void OnPlayerLogout(Player* player) override
    {
        if (!Enabled())
            return;

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Reset.OnLogout", true))
            ResetStreak(player, "logout");
    }

    void OnPlayerMapChanged(Player* player) override
    {
        if (!Enabled())
            return;

        if (sConfigMgr->GetOption<bool>("ZoeKillStreak.Reset.OnMapChange", true))
            ResetStreak(player, "mapchange");
    }
};

void AddZoeCoreKillStreakScripts()
{
    new ZoeCoreKillStreak();
}
