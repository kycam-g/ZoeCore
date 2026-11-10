#ifndef ZOE_PVP_REWARDS_H
#define ZOE_PVP_REWARDS_H

#include "Battleground.h"
#include "BattlegroundScore.h"
#include "Config.h"
#include "DatabaseEnv.h"
#include "Item.h"
#include "Mail.h"
#include "MailMgr.h"
#include "ModuleDatabasePool.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "WorldSession.h"
#include <algorithm>
#include <chrono>
#include <limits>
#include <memory>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ZoePvP
{
    using Clock = std::chrono::steady_clock;

    struct ArenaParticipant
    {
        ObjectGuid Guid;
        uint32 Account;
        TeamId Team;
    };

    struct ArenaRewardState
    {
        Clock::time_point Started = Clock::now();
        std::vector<ArenaParticipant> Participants;
        std::unordered_set<uint32> PaidAccounts;
    };

    inline uint32 MinimumCombatSeconds = 15;
    inline uint32 OpponentCooldownSeconds = 60;
    inline std::unordered_map<uint32, ArenaRewardState> ArenaStates;
    inline std::map<std::pair<uint32, uint32>, Clock::time_point> LastArenaRewards;

    inline void RecordArenaReward(Battleground* bg, Player* player)
    {
        auto& state = ArenaStates.at(bg->GetInstanceID());
        uint32 account = player->GetSession()->GetAccountId();
        state.PaidAccounts.insert(account);
        for (ArenaParticipant const& opponent : state.Participants)
            if (opponent.Team != player->GetBgTeamId())
                LastArenaRewards[{ account, opponent.Account }] = Clock::now();
    }

    // Local to one reward call. Mail, currencies and any persistent claim share one transaction.
    class Reward
    {
    public:
        explicit Reward(Player& player) : _player(player), _honor(player.GetHonorPoints()),
            _arena(player.GetArenaPoints()), _transaction(CharacterDatabase.BeginTransaction()) { }

        Reward(Reward const&) = delete;
        Reward& operator=(Reward const&) = delete;

        ~Reward()
        {
            if (_committed)
                return;

            _player.SetHonorPoints(_honor);
            _player.SetArenaPoints(_arena);
            for (uint32 id : _mailIds)
            {
                Mail* mail = _player.GetMail(id);
                for (MailItemInfo const& attachment : mail->items)
                {
                    Item* item = _player.GetMItem(attachment.item_guid);
                    _player.RemoveMItem(attachment.item_guid);
                    delete item;
                }

                _player.RemoveMail(id);
                sMailMgr->OnMailDeleted(mail->receiver);
                delete mail;
            }

            if (!_mailIds.empty())
                _player.UpdateNextMailTimeAndUnreads();
        }

        bool AddItems(uint32 entry, uint32 count)
        {
            if (!entry || !count)
                return true;

            ItemTemplate const* itemTemplate = sObjectMgr->GetItemTemplate(entry);
            if (!itemTemplate)
                return false;

            std::vector<std::unique_ptr<Item>> items;
            while (count)
            {
                uint32 stack = std::min(count, itemTemplate->GetMaxStackSize());
                std::unique_ptr<Item> item(Item::CreateItem(entry, stack, &_player));
                if (!item)
                    return false;

                items.push_back(std::move(item));
                count -= stack;
            }

            for (std::size_t offset = 0; offset < items.size();)
            {
                MailDraft draft("ZoeCore PvP - Recompensa", "Sua recompensa PvP esta nos anexos desta mensagem.");
                for (uint32 attachments = 0; attachments < MAX_MAIL_ITEMS && offset < items.size(); ++attachments)
                {
                    Item* item = items[offset++].release();
                    item->SaveToDB(_transaction);
                    draft.AddItem(item);
                }

                draft.SendMailTo(_transaction, MailReceiver(&_player), MailSender(MAIL_NORMAL, 0));
                _mailIds.push_back(_player.GetMails().front()->messageID);
            }

            return true;
        }

        void AddCurrencies(uint32 honor, uint32 arena = 0)
        {
            if (honor)
            {
                _player.SetHonorPoints(uint32(std::min<uint64>(uint64(_honor) + honor,
                    std::numeric_limits<uint32>::max())));
                auto* statement = CharacterDatabase.GetPreparedStatement(CHAR_UDP_CHAR_HONOR_POINTS);
                statement->SetData(0, _player.GetHonorPoints());
                statement->SetData(1, _player.GetGUID().GetCounter());
                _transaction->Append(statement);
            }

            if (arena)
            {
                _player.SetArenaPoints(uint32(std::min<uint64>(uint64(_arena) + arena,
                    std::numeric_limits<uint32>::max())));
                auto* statement = CharacterDatabase.GetPreparedStatement(CHAR_UDP_CHAR_ARENA_POINTS);
                statement->SetData(0, _player.GetArenaPoints());
                statement->SetData(1, _player.GetGUID().GetCounter());
                _transaction->Append(statement);
            }
        }

        CharacterDatabaseTransaction GetTransaction() const { return _transaction; }

        bool Commit(ModuleDatabasePool* database = nullptr)
        {
            if (!_transaction->GetSize())
                return _committed = true;

            // SendMailTo publishes the online mailbox immediately. Finish before processing more packets.
            auto result = database ? database->AsyncCommitTransaction(_transaction)
                : CharacterDatabase.AsyncCommitTransaction(_transaction);
            return _committed = result.m_future.get();
        }

    private:
        Player& _player;
        uint32 _honor;
        uint32 _arena;
        CharacterDatabaseTransaction _transaction;
        std::vector<uint32> _mailIds;
        bool _committed = false;
    };

    inline bool CanRewardArena(Battleground* bg, Player* player)
    {
        if (!bg || !player || !bg->isArena() || bg->GetStatus() != STATUS_WAIT_LEAVE
            || player->GetBattleground() != bg || player->IsSpectator() || player->IsGMSpectator()
            || player->isAFK() || !player->GetSession() || !bg->GetPlayers().count(player->GetGUID()))
            return false;

        auto state = ArenaStates.find(bg->GetInstanceID());
        if (state == ArenaStates.end()
            || Clock::now() - state->second.Started < std::chrono::seconds(MinimumCombatSeconds))
            return false;

        // No custom currency for surrendering without participating in combat.
        auto const* scores = bg->GetPlayerScores();
        auto score = scores->find(player->GetGUID().GetCounter());
        if (score == scores->end() || !score->second
            || (!score->second->GetDamageDone() && !score->second->GetHealingDone()))
            return false;

        uint32 account = player->GetSession()->GetAccountId();
        if (state->second.PaidAccounts.count(account))
            return false;

        for (ArenaParticipant const& opponent : state->second.Participants)
        {
            if (opponent.Guid != player->GetGUID() && opponent.Account == account)
                return false;

            auto last = LastArenaRewards.find({ account, opponent.Account });
            if (opponent.Team != player->GetBgTeamId() && last != LastArenaRewards.end()
                && Clock::now() - last->second < std::chrono::seconds(OpponentCooldownSeconds))
                return false;
        }

        return true;
    }

    class ArenaRewardConfig : public WorldScript
    {
    public:
        ArenaRewardConfig() : WorldScript("ZoePvPArenaRewardConfig", { WORLDHOOK_ON_AFTER_CONFIG_LOAD }) { }

        void OnAfterConfigLoad(bool /*reload*/) override
        {
            MinimumCombatSeconds = sConfigMgr->GetOption<uint32>("ZoePvP.Arena.Reward.MinCombatSeconds", 15);
            OpponentCooldownSeconds = sConfigMgr->GetOption<uint32>("ZoePvP.Arena.Reward.OpponentCooldownSeconds", 60);
        }
    };

    class ArenaRewardLifecycle : public AllBattlegroundScript
    {
    public:
        ArenaRewardLifecycle() : AllBattlegroundScript("ZoePvPArenaRewardLifecycle", {
            ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_START,
            ALLBATTLEGROUNDHOOK_ON_BATTLEGROUND_DESTROY
        }) { }

        void OnBattlegroundStart(Battleground* bg) override
        {
            if (!bg || !bg->isArena())
                return;

            auto [state, inserted] = ArenaStates.try_emplace(bg->GetInstanceID());
            if (!inserted)
                return;

            for (auto const& [guid, player] : bg->GetPlayers())
                if (player && player->GetSession() && !player->IsSpectator() && !player->IsGMSpectator())
                    state->second.Participants.push_back({ guid,
                        player->GetSession()->GetAccountId(), player->GetBgTeamId() });

            auto cutoff = Clock::now() - std::chrono::seconds(OpponentCooldownSeconds);
            std::erase_if(LastArenaRewards, [cutoff](auto const& entry) { return entry.second <= cutoff; });
        }

        void OnBattlegroundDestroy(Battleground* bg) override
        {
            if (bg)
                ArenaStates.erase(bg->GetInstanceID());
        }
    };

    inline void RegisterArenaRewardScripts()
    {
        static bool registered = false;
        if (registered)
            return;

        registered = true;
        new ArenaRewardConfig();
        new ArenaRewardLifecycle();
    }
}

#endif
