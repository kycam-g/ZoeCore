#include "ScriptMgr.h"
#include "Player.h"
#include "Chat.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Config.h"
#include "Creature.h"
#include "Guild.h"
#include "GuildMgr.h"
#include "Define.h"
#include "GossipDef.h"
#include "DataMap.h"
#include "GameObject.h"
#include "Transport.h"
#include "CreatureAI.h"
#include "guildhouse.h"

#include <string>


namespace
{
    bool ZoeGuildHouseEnabled()
    {
        return sConfigMgr->GetOption<bool>("GuildHouse.Enable", true);
    }

    bool ZoeGuildHouseBlockCombat(Player* player)
    {
        if (!player)
            return true;

        if (sConfigMgr->GetOption<bool>("GuildHouse.BlockInCombat", true) && player->IsInCombat())
        {
            ChatHandler(player->GetSession()).SendSysMessage("Voce nao pode usar a Casa da Guilda em combate.");
            return true;
        }

        return false;
    }

    uint32 ZoeGuildPhase(Player* player)
    {
        return player ? player->GetGuildId() + 10 : 0;
    }

    uint32 ZoeGuildHouseLevel(Player* player)
    {
        if (!player || !player->GetGuildId())
            return 0;

        QueryResult result = CharacterDatabase.Query("SELECT `level` FROM `guild_house_zoe` WHERE `guild`={}", player->GetGuildId());
        if (!result)
            return sConfigMgr->GetOption<uint32>("GuildHouse.Level.Default", 1);

        return result->Fetch()[0].Get<uint32>();
    }

    bool ZoeCheckLevel(Player* player, uint32 requiredLevel)
    {
        if (!sConfigMgr->GetOption<bool>("GuildHouse.Level.Enable", true))
            return true;

        uint32 level = ZoeGuildHouseLevel(player);
        if (level < requiredLevel)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Sua Casa da Guilda precisa ser level %u para comprar isso. Level atual: {}.", requiredLevel, level);
            return false;
        }

        return true;
    }

    void ZoeGuildHouseLog(Player* player, std::string const& action, uint32 entry, int32 goldCost, uint32 itemEntry, uint32 itemCount)
    {
        if (!player || !sConfigMgr->GetOption<bool>("GuildHouse.Log.Enable", true))
            return;

        CharacterDatabase.Execute("INSERT INTO `guild_house_log` (`guild`, `player_guid`, `account`, `action`, `entry`, `gold_cost`, `item_entry`, `item_count`, `created_at`) VALUES ({}, {}, {}, '{}', {}, {}, {}, {}, {})",
            player->GetGuildId(), player->GetGUID().GetCounter(), player->GetSession()->GetAccountId(), action, entry, goldCost, itemEntry, itemCount, uint32(time(nullptr)));
    }

    uint32 ZoeCountSpawns(Player* player, bool gameObject)
    {
        if (!player)
            return 0;

        QueryResult result = gameObject
            ? WorldDatabase.Query("SELECT COUNT(*) FROM `gameobject` WHERE `map`=1 AND `phaseMask`={}", ZoeGuildPhase(player))
            : WorldDatabase.Query("SELECT COUNT(*) FROM `creature` WHERE `map`=1 AND `phaseMask`={}", ZoeGuildPhase(player));

        if (!result)
            return 0;

        return result->Fetch()[0].Get<uint32>();
    }

    bool ZoeCheckLimit(Player* player, bool gameObject)
    {
        if (!sConfigMgr->GetOption<bool>("GuildHouse.Limit.Enable", true))
            return true;

        uint32 current = ZoeCountSpawns(player, gameObject);
        uint32 max = gameObject
            ? sConfigMgr->GetOption<uint32>("GuildHouse.Limit.MaxGameObjects", 35)
            : sConfigMgr->GetOption<uint32>("GuildHouse.Limit.MaxCreatures", 40);

        if (current >= max)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Limite da Casa da Guilda atingido: %u/{}.", current, max);
            return false;
        }

        return true;
    }

    std::string ZoeGoldText(int32 copper)
    {
        if (copper <= 0)
            return "0g";

        return std::to_string(copper / 10000) + "g";
    }

    std::string ZoeCostLabel(std::string const& text, int32 goldCost)
    {
        bool itemEnabled = sConfigMgr->GetOption<bool>("GuildHouse.UpgradeWithItem.Enable", false);
        uint32 itemEntry = sConfigMgr->GetOption<uint32>("GuildHouse.UpgradeWithItem.ItemEntry", 900001);
        uint32 itemCount = sConfigMgr->GetOption<uint32>("GuildHouse.UpgradeWithItem.ItemCount", 0);
        bool fallbackGold = sConfigMgr->GetOption<bool>("GuildHouse.UpgradeWithItem.AllowGoldFallback", true);
        bool alsoGold = sConfigMgr->GetOption<bool>("GuildHouse.UpgradeWithItem.AlsoRequireGold", false);

        if (itemEnabled && itemEntry && itemCount)
        {
            if (alsoGold)
                return text + " [" + std::to_string(itemCount) + "x item " + std::to_string(itemEntry) + " + " + ZoeGoldText(goldCost) + "]";

            if (fallbackGold)
                return text + " [" + std::to_string(itemCount) + "x item " + std::to_string(itemEntry) + " ou " + ZoeGoldText(goldCost) + "]";

            return text + " [" + std::to_string(itemCount) + "x item " + std::to_string(itemEntry) + "]";
        }

        return text + " [" + ZoeGoldText(goldCost) + "]";
    }

    bool ZoePayUpgrade(Player* player, int32 goldCost, uint32 entry, bool gameObject)
    {
        if (!player)
            return false;

        bool itemEnabled = sConfigMgr->GetOption<bool>("GuildHouse.UpgradeWithItem.Enable", false);
        uint32 itemEntry = sConfigMgr->GetOption<uint32>("GuildHouse.UpgradeWithItem.ItemEntry", 900001);
        uint32 itemCount = sConfigMgr->GetOption<uint32>("GuildHouse.UpgradeWithItem.ItemCount", 0);
        bool fallbackGold = sConfigMgr->GetOption<bool>("GuildHouse.UpgradeWithItem.AllowGoldFallback", true);
        bool alsoGold = sConfigMgr->GetOption<bool>("GuildHouse.UpgradeWithItem.AlsoRequireGold", false);

        if (itemEnabled && itemEntry && itemCount)
        {
            if (player->HasItemCount(itemEntry, itemCount, true))
            {
                if (alsoGold && player->GetMoney() < uint32(goldCost))
                {
                    ChatHandler(player->GetSession()).PSendSysMessage("Voce precisa de {} gold alem dos itens.", uint32(goldCost / 10000));
                    return false;
                }

                player->DestroyItemCount(itemEntry, itemCount, true);

                if (alsoGold && goldCost > 0)
                    player->ModifyMoney(-goldCost);

                ZoeGuildHouseLog(player, gameObject ? "BUY_OBJECT" : "BUY_NPC", entry, alsoGold ? goldCost : 0, itemEntry, itemCount);
                return true;
            }

            if (!fallbackGold)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Voce precisa de %u x item {}.", itemCount, itemEntry);
                return false;
            }
        }

        if (goldCost > 0 && player->GetMoney() < uint32(goldCost))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Voce precisa de {} gold.", uint32(goldCost / 10000));
            return false;
        }

        if (goldCost > 0)
            player->ModifyMoney(-goldCost);

        ZoeGuildHouseLog(player, gameObject ? "BUY_OBJECT" : "BUY_NPC", entry, goldCost, 0, 0);
        return true;
    }

    void ZoeSendGuildHouseInfo(Player* player)
    {
        if (!player || !player->GetGuild())
            return;

        uint32 guildId = player->GetGuildId();
        uint32 phase = ZoeGuildPhase(player);
        uint32 level = ZoeGuildHouseLevel(player);
        uint32 creatureCount = ZoeCountSpawns(player, false);
        uint32 objectCount = ZoeCountSpawns(player, true);
        std::string guildName = player->GetGuild() ? player->GetGuild()->GetName() : "Sem Guilda";

        ChatHandler chat(player->GetSession());
        chat.PSendSysMessage("===== Casa da Guilda ZoeCore =====");
        chat.PSendSysMessage("Guilda: {}", guildName);
        chat.PSendSysMessage("Guild ID: {}", guildId);
        chat.PSendSysMessage("Level: {}", level);
        chat.PSendSysMessage("Phase: {}", phase);
        chat.PSendSysMessage("NPCs: {}", creatureCount);
        chat.PSendSysMessage("Objetos: {}", objectCount);
        chat.PSendSysMessage("==================================");
    }

    void ZoeSetGuildHouseLevel(Player* player, uint32 level)
    {
        if (!player || !player->GetGuildId())
            return;

        CharacterDatabase.Execute("REPLACE INTO `guild_house_zoe` (`guild`, `level`, `updated_at`) VALUES ({}, {}, {})",
            player->GetGuildId(), level, uint32(time(nullptr)));
    }

    bool ZoePayHouseUpgrade(Player* player, uint32 targetLevel)
    {
        uint32 maxLevel = sConfigMgr->GetOption<uint32>("GuildHouse.Level.Max", 3);
        if (targetLevel < 2 || targetLevel > maxLevel)
            return false;

        uint32 currentLevel = ZoeGuildHouseLevel(player);
        if (currentLevel + 1 != targetLevel)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Voce precisa evoluir a Casa da Guilda em ordem. Level atual: {}.", currentLevel);
            return false;
        }

        uint32 itemEntry = sConfigMgr->GetOption<uint32>("GuildHouse.Level.Upgrade.ItemEntry", 900001);
        uint32 itemCount = targetLevel == 2
            ? sConfigMgr->GetOption<uint32>("GuildHouse.Level.Upgrade.Level2.ItemCount", 150)
            : sConfigMgr->GetOption<uint32>("GuildHouse.Level.Upgrade.Level3.ItemCount", 300);

        int32 goldCost = targetLevel == 2
            ? sConfigMgr->GetOption<int32>("GuildHouse.Level.Upgrade.Level2.GoldCost", 10000000)
            : sConfigMgr->GetOption<int32>("GuildHouse.Level.Upgrade.Level3.GoldCost", 25000000);

        bool fallbackGold = sConfigMgr->GetOption<bool>("GuildHouse.Level.Upgrade.AllowGoldFallback", true);

        if (itemEntry && itemCount && player->HasItemCount(itemEntry, itemCount, true))
        {
            player->DestroyItemCount(itemEntry, itemCount, true);
            ZoeSetGuildHouseLevel(player, targetLevel);
            ZoeGuildHouseLog(player, "UPGRADE_LEVEL_NPC", targetLevel, 0, itemEntry, itemCount);
            return true;
        }

        if (fallbackGold && goldCost > 0 && player->GetMoney() >= uint32(goldCost))
        {
            player->ModifyMoney(-goldCost);
            ZoeSetGuildHouseLevel(player, targetLevel);
            ZoeGuildHouseLog(player, "UPGRADE_LEVEL_NPC", targetLevel, goldCost, 0, 0);
            return true;
        }

        ChatHandler(player->GetSession()).SendSysMessage("Voce nao possui os requisitos configurados para evoluir a Casa da Guilda.");
        return false;
    }

}


int cost, GuildHouseInnKeeper, GuildHouseBank, GuildHouseMailBox, GuildHouseAuctioneer, GuildHouseTrainer, GuildHouseVendor, GuildHouseObject, GuildHousePortal, GuildHouseSpirit, GuildHouseProf, GuildHouseBuyRank;

class GuildHouseSpawner : public CreatureScript
{

public:
    GuildHouseSpawner() : CreatureScript("GuildHouseSpawner") {}

    struct GuildHouseSpawnerAI : public ScriptedAI
    {
        GuildHouseSpawnerAI(Creature* creature) : ScriptedAI(creature) {}

        void UpdateAI(uint32 /*diff*/) override
        {
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        }
    };

    CreatureAI* GetAI(Creature *creature) const override
    {
        return new GuildHouseSpawnerAI(creature);
    }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!ZoeGuildHouseEnabled())
        {
            ChatHandler(player->GetSession()).SendSysMessage("O sistema de Casa da Guilda esta desativado.");
            CloseGossipMenuFor(player);
            return false;
        }

        if (ZoeGuildHouseBlockCombat(player))
        {
            CloseGossipMenuFor(player);
            return false;
        }

        if (player->GetGuild())
        {
            Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
            Guild::Member const* memberMe = guild->GetMember(player->GetGUID());

            if (!memberMe->IsRankNotLower(GuildHouseBuyRank))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Voce nao tem permissao para comprar melhorias da Casa da Guilda.");
                return false;
            }
        }
        else
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Voce nao esta em uma guilda!");
            return false;
        }

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Informacoes da Guilda", GOSSIP_SENDER_MAIN, 11);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Aumentar Nivel da Casa", GOSSIP_SENDER_MAIN, 12);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "Innkeeper", GOSSIP_SENDER_MAIN, GetCreatureEntry(2), "Adicionar um Estalajadeiro?", GuildHouseInnKeeper, false);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "Mailbox", GOSSIP_SENDER_MAIN, 184137, "Adicionar uma Caixa de Correio?", GuildHouseMailBox, false);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "Stable Master", GOSSIP_SENDER_MAIN, 28690, "Adicionar um Mestre de Estabulo?", GuildHouseVendor, false);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "Treinador de Classe", GOSSIP_SENDER_MAIN, 2);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "Vendedores", GOSSIP_SENDER_MAIN, 3);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "Portais / Objetos", GOSSIP_SENDER_MAIN, 4);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Bank", GOSSIP_SENDER_MAIN, 30605, "Adicionar um Banqueiro?", GuildHouseBank, false);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Auctioneer", GOSSIP_SENDER_MAIN, 6, "Adicionar um Leiloeiro?", GuildHouseAuctioneer, false);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Neutral Auctioneer", GOSSIP_SENDER_MAIN, 9858, "Adicionar um Leiloeiro Neutro?", GuildHouseAuctioneer, false);
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Profissoes Primarias", GOSSIP_SENDER_MAIN, 7);
        AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Profissoes Secundarias", GOSSIP_SENDER_MAIN, 8);
        AddGossipItemFor(player, GOSSIP_ICON_TALK, "Spirit Healer", GOSSIP_SENDER_MAIN, 6491, "Adicionar um Spirit Healer?", GuildHouseSpirit, false);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!ZoeGuildHouseEnabled())
            return false;

        if (ZoeGuildHouseBlockCombat(player))
        {
            CloseGossipMenuFor(player);
            return false;
        }

        switch (action)
        {
        case 11: // guild info
            ZoeSendGuildHouseInfo(player);
            CloseGossipMenuFor(player);
            break;
        case 12: // upgrade house level
        {
            uint32 currentLevel = ZoeGuildHouseLevel(player);
            uint32 nextLevel = currentLevel + 1;
            uint32 maxLevel = sConfigMgr->GetOption<uint32>("GuildHouse.Level.Max", 3);

            if (nextLevel > maxLevel)
            {
                ChatHandler(player->GetSession()).SendSysMessage("Sua Casa da Guilda ja esta no level maximo.");
                CloseGossipMenuFor(player);
                break;
            }

            if (ZoePayHouseUpgrade(player, nextLevel))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Casa da Guilda evoluida para level {}.", nextLevel);
                player->GetGuild()->BroadcastToGuild(player->GetSession(), false, "A Casa da Guilda foi evoluida!", LANG_UNIVERSAL);
            }

            CloseGossipMenuFor(player);
            break;
        }
        case 2: // Adicionar Treinador de Classe
            ClearGossipMenuFor(player);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Death Knight", GOSSIP_SENDER_MAIN, 29195, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Druid", GOSSIP_SENDER_MAIN, 26324, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Hunter", GOSSIP_SENDER_MAIN, 26325, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Mage", GOSSIP_SENDER_MAIN, 26326, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Paladin", GOSSIP_SENDER_MAIN, 26327, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Priest", GOSSIP_SENDER_MAIN, 26328, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Rogue", GOSSIP_SENDER_MAIN, 26329, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Shaman", GOSSIP_SENDER_MAIN, 26330, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Warlock", GOSSIP_SENDER_MAIN, 26331, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Warrior", GOSSIP_SENDER_MAIN, 26332, "Adicionar?", GuildHouseTrainer, false);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Voltar", GOSSIP_SENDER_MAIN, 9);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            break;
        case 3: // Vendors
            ClearGossipMenuFor(player);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "Trade Supplies", GOSSIP_SENDER_MAIN, 28692, "Adicionar?", GuildHouseVendor, false);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "Tabards", GOSSIP_SENDER_MAIN, 28776, "Adicionar?", GuildHouseVendor, false);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "Food & Drink", GOSSIP_SENDER_MAIN, 19572, "Adicionar?", GuildHouseVendor, false);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "Reagents", GOSSIP_SENDER_MAIN, 29636, "Adicionar?", GuildHouseVendor, false);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "Ammo & Repair", GOSSIP_SENDER_MAIN, 29493, "Adicionar?", GuildHouseVendor, false);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "Poisons", GOSSIP_SENDER_MAIN, 2622, "Adicionar?", GuildHouseVendor, false);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Voltar", GOSSIP_SENDER_MAIN, 9);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            break;
        case 4: // Objects & Portals
            ClearGossipMenuFor(player);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "Forge", GOSSIP_SENDER_MAIN, 1685, "Adicionar uma Forja?", GuildHouseObject, false);
            AddGossipItemFor(player, GOSSIP_ICON_TALK, "Anvil", GOSSIP_SENDER_MAIN, 4087, "Add an Bigorna?", GuildHouseObject, false);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Guild Vault", GOSSIP_SENDER_MAIN, 187293, "Add Cofre da Guilda?", GuildHouseObject, false);
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "Barber Chair", GOSSIP_SENDER_MAIN, 191028, "Add a Cadeira de Barbeiro?", GuildHouseObject, false);

            if (player->GetTeamId() == TEAM_ALLIANCE)
            {
                // ALLIANCE players get these options
                AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Portal: Ironforge", GOSSIP_SENDER_MAIN, GetGameObjectEntry(3), "Adicionar Portal para Ironforge?", GuildHousePortal, false);
                AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Portal: Darnassus", GOSSIP_SENDER_MAIN, GetGameObjectEntry(1), "Adicionar Portal para Darnassus?", GuildHousePortal, false);
                AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Portal: Exodar", GOSSIP_SENDER_MAIN, GetGameObjectEntry(2), "Adicionar Portal para Exodar?", GuildHousePortal, false);
            }
            else
            {
                // HORDE players get these options
                AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Portal: Undercity", GOSSIP_SENDER_MAIN, GetGameObjectEntry(7), "Adicionar Portal para Undercity?", GuildHousePortal, false);
                AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Portal: Thunderbluff", GOSSIP_SENDER_MAIN, GetGameObjectEntry(6), "Adicionar Portal para Thunder Bluff?", GuildHousePortal, false);
                AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Portal: Silvermoon", GOSSIP_SENDER_MAIN, GetGameObjectEntry(5), "Adicionar Portal para Silvermoon?", GuildHousePortal, false);
            }

            // These two portals work for either Team
            AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Portal: Shattrath", GOSSIP_SENDER_MAIN, GetGameObjectEntry(8), "Adicionar Portal para Shattrath?", GuildHousePortal, false);
            AddGossipItemFor(player, GOSSIP_ICON_TAXI, "Portal: Dalaran", GOSSIP_SENDER_MAIN, GetGameObjectEntry(9), "Adicionar Portal para Dalaran?", GuildHousePortal, false);

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Voltar", GOSSIP_SENDER_MAIN, 9);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            break;
        case 6: // Auctioneer
        {
            uint32 auctioneer = player->GetTeamId() == TEAM_ALLIANCE ? 8719 : 9856;
            cost = GuildHouseAuctioneer;
            SpawnNPC(auctioneer, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Auctioneer", 2));
            break;
        }
        case 9858: // Neutral Auctioneer
            cost = GuildHouseAuctioneer;
            SpawnNPC(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Auctioneer", 2));
            break;
        case 7: // Spawn Profession Trainers
            ClearGossipMenuFor(player);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Alchemy", GOSSIP_SENDER_MAIN, 19052, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Blacksmithing", GOSSIP_SENDER_MAIN, 2836, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Engineering", GOSSIP_SENDER_MAIN, 8736, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Tailoring", GOSSIP_SENDER_MAIN, 2627, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Leatherworking", GOSSIP_SENDER_MAIN, 19187, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Skinning", GOSSIP_SENDER_MAIN, 19180, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Mining", GOSSIP_SENDER_MAIN, 8128, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Herbalism", GOSSIP_SENDER_MAIN, 908, "Adicionar?", GuildHouseProf, false);

            if (player->GetTeamId() == TEAM_ALLIANCE)
            {
                // ALLIANCE players get these options
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Enchanting", GOSSIP_SENDER_MAIN, 18773, "Adicionar?", GuildHouseProf, false);
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Jewelcrafting", GOSSIP_SENDER_MAIN, 18774, "Adicionar?", GuildHouseProf, false);
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Inscription", GOSSIP_SENDER_MAIN, 30721, "Adicionar?", GuildHouseProf, false);
            }
            else
            {
                // HORDE players get these options
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Enchanting", GOSSIP_SENDER_MAIN, 18753, "Adicionar?", GuildHouseProf, false);
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Jewelcrafting", GOSSIP_SENDER_MAIN, 18751, "Adicionar?", GuildHouseProf, false);
                AddGossipItemFor(player, GOSSIP_ICON_TRAINER, "Inscription", GOSSIP_SENDER_MAIN, 30722, "Adicionar?", GuildHouseProf, false);
            }

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Voltar", GOSSIP_SENDER_MAIN, 9);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            break;
        case 8: // Secondary Profession Trainers
            ClearGossipMenuFor(player);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "First Aid", GOSSIP_SENDER_MAIN, 19184, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Fishing", GOSSIP_SENDER_MAIN, 2834, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Cooking", GOSSIP_SENDER_MAIN, 19185, "Adicionar?", GuildHouseProf, false);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Voltar", GOSSIP_SENDER_MAIN, 9);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            break;
        case 9: // Go back!
            OnGossipHello(player, creature);
            break;
        case 10: // PVP toggle
            break;
        case 30605: // Banker
            cost = GuildHouseBank;
            SpawnNPC(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Bank", 1));
            break;
        case GetCreatureEntry(2): // Innkeeper
            cost = GuildHouseInnKeeper;
            SpawnNPC(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.InnKeeper", 1));
            break;
        case 26327: // Paladino
        case 26324: // Druida
        case 26325: // Cacador
        case 26326: // Mago
        case 26328: // Sacerdote
        case 26329: // Ladino
        case 26330: // Shaman
        case 26331: // Bruxo
        case 26332: // Guerreiro
        case 29195: // Death Knight
            cost = GuildHouseTrainer;
            SpawnNPC(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Trainer", 2));
            break;
        case 2836:  // Blacksmithing
        case 8128:  // Mining
        case 8736:  // Engineering
        case 18774: // Jewelcrafting (Alliance)
        case 18751: // Jewelcrafting (Horde)
        case 18773: // Enchanting (Alliance)
        case 18753: // Enchanting (Horde)
        case 30721: // Inscription (Alliance)
        case 30722: // Inscription (Horde)
        case 19187: // Leatherworking
        case 19180: // Skinning
        case 19052: // Alchemy
        case 908:   // Herbalism
        case 2627:  // Tailoring
        case 19185: // Cooking
        case 2834:  // Fishing
        case 19184: // First Aid
            cost = GuildHouseProf;
            SpawnNPC(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Profession", 2));
            break;
        case 28692: // Suprimentos Comerciais
        case 28776: // Vendedor de Tabardos
        case 19572:  // Vendedor de Comida e Bebida
        case 29636: // Vendedor de Reagentes
        case 29493: // Vendedor de Municao e Reparo
        case 28690: // Stable Master
        case 2622:  // Vendedor de Venenos
            cost = GuildHouseVendor;
            SpawnNPC(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Vendor", 1));
            break;
        //
        // Objects
        //
        case 184137: // Mailbox
            cost = GuildHouseMailBox;
            SpawnObject(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Mailbox", 1));
            break;
        case 6491: // Spirit Healer
            cost = GuildHouseSpirit;
            SpawnNPC(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Spirit", 3));
            break;
        case 1685:   // Forja
        case 4087:   // Bigorna
        case 187293: // Cofre da Guilda
        case 191028: // Cadeira de Barbeiro
            cost = GuildHouseObject;
            SpawnObject(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Object", 1));
            break;
        case GetGameObjectEntry(1): // Darnassus Portal
        case GetGameObjectEntry(2): // Exodar Portal
        case GetGameObjectEntry(3): // Ironforge Portal
        case GetGameObjectEntry(5): // Silvermoon Portal
        case GetGameObjectEntry(6): // Thunder Bluff Portal
        case GetGameObjectEntry(7): // Undercity Portal
        case GetGameObjectEntry(8): // Shattrath Portal
        case GetGameObjectEntry(9): // Dalaran Portal
            cost = GuildHousePortal;
            SpawnObject(action, player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Required.Portal", 2));
            break;
        }
        return true;
    }

    uint32 GetGuildPhase(Player* player)
    {
        return player->GetGuildId() + 10;
    }

    void SpawnNPC(uint32 entry, Player* player, uint32 requiredLevel)
    {
        if (!ZoeCheckLevel(player, requiredLevel))
        {
            CloseGossipMenuFor(player);
            return;
        }

        if (!ZoeCheckLimit(player, false))
        {
            CloseGossipMenuFor(player);
            return;
        }

        if (!ZoePayUpgrade(player, cost, entry, false))
        {
            CloseGossipMenuFor(player);
            return;
        }

        if (player->FindNearestCreature(entry, VISIBILITY_RANGE, true))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Voce ja possui este NPC!");
            CloseGossipMenuFor(player);
            return;
        }

        float posX;
        float posY;
        float posZ;
        float ori;

        QueryResult result = WorldDatabase.Query("SELECT `posX`, `posY`, `posZ`, `orientation` FROM `guild_house_spawns` WHERE `entry`={}", entry);

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            posX = fields[0].Get<float>();
            posY = fields[1].Get<float>();
            posZ = fields[2].Get<float>();
            ori = fields[3].Get<float>();

        } while (result->NextRow());

        Creature* creature = new Creature();

        if (!creature->Create(player->GetMap()->GenerateLowGuid<HighGuid::Unit>(), player->GetMap(), GetGuildPhase(player), entry, 0, posX, posY, posZ, ori))
        {
            delete creature;
            return;
        }
        creature->SaveToDB(player->GetMapId(), (1 << player->GetMap()->GetSpawnMode()), GetGuildPhase(player));
        uint32 db_guid = creature->GetSpawnId();

        creature->CleanupsBeforeDelete();
        delete creature;
        creature = new Creature();
        if (!creature->LoadCreatureFromDB(db_guid, player->GetMap()))
        {
            delete creature;
            return;
        }

        sObjectMgr->AddCreatureToGrid(db_guid, sObjectMgr->GetCreatureData(db_guid));
        CloseGossipMenuFor(player);
    }

    void SpawnObject(uint32 entry, Player* player, uint32 requiredLevel)
    {
        if (!ZoeCheckLevel(player, requiredLevel))
        {
            CloseGossipMenuFor(player);
            return;
        }

        if (!ZoeCheckLimit(player, true))
        {
            CloseGossipMenuFor(player);
            return;
        }

        if (!ZoePayUpgrade(player, cost, entry, true))
        {
            CloseGossipMenuFor(player);
            return;
        }

        if (player->FindNearestGameObject(entry, VISIBLE_RANGE))
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Voce ja possui este objeto!");
            CloseGossipMenuFor(player);
            return;
        }

        float posX;
        float posY;
        float posZ;
        float ori;

        QueryResult result = WorldDatabase.Query("SELECT `posX`, `posY`, `posZ`, `orientation` FROM `guild_house_spawns` WHERE `entry`={}", entry);

        if (!result)
            return;

        do
        {
            Field* fields = result->Fetch();
            posX = fields[0].Get<float>();
            posY = fields[1].Get<float>();
            posZ = fields[2].Get<float>();
            ori = fields[3].Get<float>();

        } while (result->NextRow());

        uint32 objectId = entry;
        if (!objectId)
            return;

        const GameObjectTemplate* objectInfo = sObjectMgr->GetGameObjectTemplate(objectId);

        if (!objectInfo)
            return;

        if (objectInfo->displayId && !sGameObjectDisplayInfoStore.LookupEntry(objectInfo->displayId))
            return;

        GameObject* object = sObjectMgr->IsGameObjectStaticTransport(objectInfo->entry) ? new StaticTransport() : new GameObject();
        ObjectGuid::LowType guidLow = player->GetMap()->GenerateLowGuid<HighGuid::GameObject>();

        if (!object->Create(guidLow, objectInfo->entry, player->GetMap(), GetGuildPhase(player), posX, posY, posZ, ori, G3D::Quat(), 0, GO_STATE_READY))
        {
            delete object;
            return;
        }

        // fill the gameobject data and save to the db
        object->SaveToDB(player->GetMapId(), (1 << player->GetMap()->GetSpawnMode()), GetGuildPhase(player));
        guidLow = object->GetSpawnId();
        // delete the old object and do a clean load from DB with a fresh new GameObject instance.
        // this is required to avoid weird behavior and memory leaks
        delete object;

        object = sObjectMgr->IsGameObjectStaticTransport(objectInfo->entry) ? new StaticTransport() : new GameObject();
        // this will generate a new guid if the object is in an instance
        if (!object->LoadGameObjectFromDB(guidLow, player->GetMap(), true))
        {
            delete object;
            return;
        }

        // TODO: is it really necessary to add both the real and DB table guid here ?
        sObjectMgr->AddGameobjectToGrid(guidLow, sObjectMgr->GetGameObjectData(guidLow));
        CloseGossipMenuFor(player);
    }
};

class GuildHouseButlerConf : public WorldScript
{
public:
    GuildHouseButlerConf() : WorldScript("GuildHouseButlerConf") {}

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        GuildHouseInnKeeper = sConfigMgr->GetOption<int32>("GuildHouseInnKeeper", 1000000);
        GuildHouseBank = sConfigMgr->GetOption<int32>("GuildHouseBank", 1000000);
        GuildHouseMailBox = sConfigMgr->GetOption<int32>("GuildHouseMailbox", 500000);
        GuildHouseAuctioneer = sConfigMgr->GetOption<int32>("GuildHouseAuctioneer", 500000);
        GuildHouseTrainer = sConfigMgr->GetOption<int32>("GuildHouseTrainerCost", 1000000);
        GuildHouseVendor = sConfigMgr->GetOption<int32>("GuildHouseVendor", 500000);
        GuildHouseObject = sConfigMgr->GetOption<int32>("GuildHouseObject", 500000);
        GuildHousePortal = sConfigMgr->GetOption<int32>("GuildHousePortal", 500000);
        GuildHouseProf = sConfigMgr->GetOption<int32>("GuildHouseProf", 500000);
        GuildHouseSpirit = sConfigMgr->GetOption<int32>("GuildHouseSpirit", 100000);
        GuildHouseBuyRank = sConfigMgr->GetOption<int32>("GuildHouseBuyRank", 4);
    }
};

void AddGuildHouseButlerScripts()
{
    new GuildHouseSpawner();
    new GuildHouseButlerConf();
}
