#include "ScriptMgr.h"
#include "Player.h"
#include "Config.h"
#include "Creature.h"
#include "Guild.h"
#include "SpellAuraEffects.h"
#include "Chat.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "GuildMgr.h"
#include "Define.h"
#include "GossipDef.h"
#include "DataMap.h"
#include "GameObject.h"
#include "Transport.h"
#include "MapMgr.h"
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

    void ZoeSetGuildHouseLevel(Player* player, uint32 level)
    {
        if (!player || !player->GetGuildId())
            return;

        CharacterDatabase.Execute("REPLACE INTO `guild_house_zoe` (`guild`, `level`, `updated_at`) VALUES ({}, {}, {})",
            player->GetGuildId(), level, uint32(time(nullptr)));
    }

    void ZoeGuildHouseLog(Player* player, std::string const& action, uint32 entry, int32 goldCost, uint32 itemEntry, uint32 itemCount)
    {
        if (!player || !sConfigMgr->GetOption<bool>("GuildHouse.Log.Enable", true))
            return;

        CharacterDatabase.Execute("INSERT INTO `guild_house_log` (`guild`, `player_guid`, `account`, `action`, `entry`, `gold_cost`, `item_entry`, `item_count`, `created_at`) VALUES ({}, {}, {}, '{}', {}, {}, {}, {}, {})",
            player->GetGuildId(), player->GetGUID().GetCounter(), player->GetSession()->GetAccountId(), action, entry, goldCost, itemEntry, itemCount, uint32(time(nullptr)));
    }

    std::string ZoeGoldText(int32 copper)
    {
        if (copper <= 0)
            return "0g";

        return std::to_string(copper / 10000) + "g";
    }

    std::string ZoeCostLabel(std::string const& text, int32 goldCost, bool house)
    {
        std::string prefix = house ? "GuildHouse.BuyWithItem." : "GuildHouse.UpgradeWithItem.";
        bool itemEnabled = sConfigMgr->GetOption<bool>(prefix + "Enable", false);
        uint32 itemEntry = sConfigMgr->GetOption<uint32>(prefix + "ItemEntry", 900001);
        uint32 itemCount = sConfigMgr->GetOption<uint32>(prefix + "ItemCount", 0);
        bool fallbackGold = sConfigMgr->GetOption<bool>(prefix + "AllowGoldFallback", true);
        bool alsoGold = sConfigMgr->GetOption<bool>(prefix + "AlsoRequireGold", false);

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

    bool ZoePay(Player* player, int32 goldCost, bool house, std::string const& action, uint32 entry)
    {
        if (!player)
            return false;

        std::string prefix = house ? "GuildHouse.BuyWithItem." : "GuildHouse.UpgradeWithItem.";
        bool itemEnabled = sConfigMgr->GetOption<bool>(prefix + "Enable", false);
        uint32 itemEntry = sConfigMgr->GetOption<uint32>(prefix + "ItemEntry", 900001);
        uint32 itemCount = sConfigMgr->GetOption<uint32>(prefix + "ItemCount", 0);
        bool fallbackGold = sConfigMgr->GetOption<bool>(prefix + "AllowGoldFallback", true);
        bool alsoGold = sConfigMgr->GetOption<bool>(prefix + "AlsoRequireGold", false);

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

                ZoeGuildHouseLog(player, action, entry, alsoGold ? goldCost : 0, itemEntry, itemCount);
                return true;
            }

            if (!fallbackGold)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Voce precisa de {} x item {}.", itemCount, itemEntry);
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

        ZoeGuildHouseLog(player, action, entry, goldCost, 0, 0);
        return true;
    }

    bool ZoePayHouseUpgrade(Player* player, uint32 targetLevel)
    {
        uint32 maxLevel = sConfigMgr->GetOption<uint32>("GuildHouse.Level.Max", 3);
        if (targetLevel < 2 || targetLevel > maxLevel)
            return false;

        uint32 currentLevel = ZoeGuildHouseLevel(player);
        if (currentLevel + 1 != targetLevel)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Voce precisa evoluir a Casa da Guilda em ordem. Level atual: %u.", currentLevel);
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
            ZoeGuildHouseLog(player, "UPGRADE_LEVEL", targetLevel, 0, itemEntry, itemCount);
            return true;
        }

        if (fallbackGold && goldCost > 0 && player->GetMoney() >= uint32(goldCost))
        {
            player->ModifyMoney(-goldCost);
            ZoeSetGuildHouseLevel(player, targetLevel);
            ZoeGuildHouseLog(player, "UPGRADE_LEVEL", targetLevel, goldCost, 0, 0);
            return true;
        }

        ChatHandler(player->GetSession()).PSendSysMessage("Voce precisa dos itens configurados ou gold suficiente para evoluir.");
        return false;
    }

    bool ZoeIsGuildOfficerEnough(Player* player, int32 rank)
    {
        if (!player || !player->GetGuild())
            return false;

        Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
        if (!guild)
            return false;

        Guild::Member const* member = guild->GetMember(player->GetGUID());
        return member && member->IsRankNotLower(rank);
    }

    void ZoeSendGuildHouseInfo(Player* player, ChatHandler* handler = nullptr)
    {
        if (!player || !player->GetGuild())
            return;

        uint32 guildId = player->GetGuildId();
        uint32 phase = ZoeGuildPhase(player);
        uint32 level = ZoeGuildHouseLevel(player);
        uint32 creatureCount = 0;
        uint32 objectCount = 0;

        if (QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM `creature` WHERE `map`=1 AND `phaseMask`={}", phase))
            creatureCount = result->Fetch()[0].Get<uint32>();

        if (QueryResult result = WorldDatabase.Query("SELECT COUNT(*) FROM `gameobject` WHERE `map`=1 AND `phaseMask`={}", phase))
            objectCount = result->Fetch()[0].Get<uint32>();

        std::string guildName = player->GetGuild() ? player->GetGuild()->GetName() : "Sem Guilda";

        ChatHandler chat = handler ? *handler : ChatHandler(player->GetSession());
        chat.PSendSysMessage("===== Casa da Guilda ZoeCore =====");
        chat.PSendSysMessage("Guilda: {}", guildName);
        chat.PSendSysMessage("Guild ID: {}", guildId);
        chat.PSendSysMessage("Level: {}", level);
        chat.PSendSysMessage("Phase: {}", phase);
        chat.PSendSysMessage("NPCs: {}", creatureCount);
        chat.PSendSysMessage("Objetos: {}", objectCount);
        chat.PSendSysMessage("==================================");
    }

}



class GuildData : public DataMap::Base
{
public:
    GuildData() {}
    GuildData(uint32 phase, float posX, float posY, float posZ, float ori) : phase(phase), posX(posX), posY(posY), posZ(posZ), ori(ori) {}
    uint32 phase;
    float posX;
    float posY;
    float posZ;
    float ori;
};

class GuildHelper : public GuildScript
{

public:
    GuildHelper() : GuildScript("GuildHelper") {}

    void OnCreate(Guild* /*guild*/, Player* leader, const std::string& /*name*/)
    {
        ChatHandler(leader->GetSession()).PSendSysMessage("Agora voce possui uma guilda. Voce ja pode comprar uma Casa da Guilda!");
    }

    uint32 GetGuildPhase(Guild* guild)
    {
        return guild->GetId() + 10;
    }

    void OnDisband(Guild* guild)
    {

        if (RemoveGuildHouse(guild))
        {
            LOG_INFO("modules", "GUILDHOUSE: Deleting Guild House data due to disbanding of guild...");
        }
        else
        {
            LOG_INFO("modules", "GUILDHOUSE: Error deleting Guild House data during disbanding of guild!!");
        }
    }

    bool RemoveGuildHouse(Guild* guild)
    {
        uint32 guildPhase = GetGuildPhase(guild);
        QueryResult CreatureResult;
        QueryResult GameobjResult;

        // Lets find all of the gameobjects to be removed
        GameobjResult = WorldDatabase.Query("SELECT `guid` FROM `gameobject` WHERE `map`=1 AND `phaseMask`={}", guildPhase);
        // Lets find all of the creatures to be removed
        CreatureResult = WorldDatabase.Query("SELECT `guid` FROM `creature` WHERE `map`=1 AND `phaseMask`={}", guildPhase);

        Map* map = sMapMgr->FindMap(1, 0);
        // Remove creatures from the deleted guild house map
        if (CreatureResult)
        {
            do
            {
                Field* fields = CreatureResult->Fetch();
                uint32 lowguid = fields[0].Get<int32>();
                if (CreatureData const* cr_data = sObjectMgr->GetCreatureData(lowguid))
                {
                    if (Creature* creature = map->GetCreature(ObjectGuid::Create<HighGuid::Unit>(cr_data->id1, lowguid)))
                    {
                        creature->CombatStop();
                        creature->DeleteFromDB();
                        creature->AddObjectToRemoveList();
                    }
                }
            } while (CreatureResult->NextRow());
        }

        // Remove gameobjects from the deleted guild house map
        if (GameobjResult)
        {
            do
            {
                Field *fields = GameobjResult->Fetch();
                uint32 lowguid = fields[0].Get<int32>();
                if (GameObjectData const* go_data = sObjectMgr->GetGameObjectData(lowguid))
                {
                    if (GameObject* gobject = map->GetGameObject(ObjectGuid::Create<HighGuid::GameObject>(go_data->id, lowguid)))
                    {
                        gobject->SetRespawnTime(0);
                        gobject->Delete();
                        gobject->DeleteFromDB();
                        gobject->CleanupsBeforeDelete();
                        // delete gobject;
                    }
                }

            } while (GameobjResult->NextRow());
        }

        // Delete actual guild_house data from characters database
        CharacterDatabase.Query("DELETE FROM `guild_house` WHERE `guild`={}", guild->GetId());

        return true;
    }
};

class GuildHouseSeller : public CreatureScript
{

public:
    GuildHouseSeller() : CreatureScript("GuildHouseSeller") {}

    struct GuildHouseSellerAI : public ScriptedAI
    {
        GuildHouseSellerAI(Creature* creature) : ScriptedAI(creature) {}

        void UpdateAI(uint32 /*diff*/) override
        {
            me->SetFlag(UNIT_NPC_FLAGS, UNIT_NPC_FLAG_GOSSIP);
        }
    };

    CreatureAI * GetAI(Creature* creature) const override
    {
        return new GuildHouseSellerAI(creature);
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

        if (!player->GetGuild())
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Voce nao e membro de uma guilda.");
            CloseGossipMenuFor(player);
            return false;
        }

        QueryResult has_gh = CharacterDatabase.Query("SELECT id, `guild` FROM `guild_house` WHERE guild = {}", player->GetGuildId());

        // Only show Teleport option if guild owns a guild house
        if (has_gh)
        {
            AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Teleportar para Casa da Guilda", GOSSIP_SENDER_MAIN, 1);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Informacoes da Guilda", GOSSIP_SENDER_MAIN, 6);

            // Only show "Sell" option if they have a guild house & have permission to sell it
            Guild* guild = sGuildMgr->GetGuildById(player->GetGuildId());
            Guild::Member const* memberMe = guild->GetMember(player->GetGUID());
            if (memberMe->IsRankNotLower(sConfigMgr->GetOption<int32>("GuildHouseSellRank", 0)))
            {
                AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Vender Casa da Guilda", GOSSIP_SENDER_MAIN, 3, "Tem certeza que deseja vender sua Casa da Guilda?", 0, false);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Aumentar Nivel da Casa", GOSSIP_SENDER_MAIN, 7, "Deseja aumentar o nivel da Casa da Guilda?", 0, false);
            }
        }
        else
        {
            // Only leader of the guild can buy guild house & only if they don't already have a guild house
            if (player->GetGuild()->GetLeaderGUID() == player->GetGUID())
            {
                AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Comprar Casa da Guilda", GOSSIP_SENDER_MAIN, 2);
            }
        }

        AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Fechar", GOSSIP_SENDER_MAIN, 5);
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

        uint32 map;
        float posX;
        float posY;
        float posZ;
        float ori;

        switch (action)
        {
        case 100: // Ilha GM
            map = 1;
            posX = 16222.972f;
            posY = 16267.802f;
            posZ = 13.136777f;
            ori = 1.461173f;
            break;
        case 5: // close
            CloseGossipMenuFor(player);
            break;
        case 6: // guild info
            ZoeSendGuildHouseInfo(player);
            CloseGossipMenuFor(player);
            break;
        case 7: // upgrade guild house
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
        case 4: // --- MORE TO COME ---
            BuyGuildHouse(player->GetGuild(), player, creature);
            break;
        case 3: // sell back guild house
        {
            QueryResult has_gh = CharacterDatabase.Query("SELECT id, `guild` FROM `guild_house` WHERE guild={}", player->GetGuildId());
            if (!has_gh)
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Sua guilda nao possui uma Casa da Guilda!");
                CloseGossipMenuFor(player);
                return false;
            }

            // calculate total gold returned: 1) cost of guild house and cost of each purchase made
            if (RemoveGuildHouse(player))
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Voce vendeu a Casa da Guilda com sucesso.");
                player->GetGuild()->BroadcastToGuild(player->GetSession(), false, "Acabamos de vender nossa Casa da Guilda.", LANG_UNIVERSAL);
                player->ModifyMoney(+(sConfigMgr->GetOption<int32>("CostGuildHouse", 10000000) / 2));
                LOG_INFO("modules", "GUILDHOUSE: Successfully returned money and sold Guild House");
                CloseGossipMenuFor(player);
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Ocorreu um erro ao vender sua Casa da Guilda.");
                CloseGossipMenuFor(player);
            }
            break;
        }
        case 2: // buy guild house
            BuyGuildHouse(player->GetGuild(), player, creature);
            break;
        case 1: // teleport to guild house
            TeleportGuildHouse(player->GetGuild(), player, creature);
            break;
        }

        if (action >= 100)
        {
            int32 houseCost = sConfigMgr->GetOption<int32>("CostGuildHouse", 10000000);

            if (!ZoePay(player, houseCost, true, "BUY_HOUSE", action))
            {
                CloseGossipMenuFor(player);
                return false;
            }

            CharacterDatabase.Execute("INSERT INTO `guild_house` (guild, phase, map, positionX, positionY, positionZ, orientation) VALUES ({}, {}, {}, {}, {}, {}, {})", player->GetGuildId(), GetGuildPhase(player), map, posX, posY, posZ, ori);
            ZoeSetGuildHouseLevel(player, sConfigMgr->GetOption<uint32>("GuildHouse.Level.Default", 1));

            ChatHandler(player->GetSession()).PSendSysMessage("Voce comprou uma Casa da Guilda com sucesso.");
            player->GetGuild()->BroadcastToGuild(player->GetSession(), false, "Agora temos uma Casa da Guilda!", LANG_UNIVERSAL);
            player->GetGuild()->BroadcastToGuild(player->GetSession(), false, "No chat, digite `.guildhouse teleport` ou `.gh tele` para ir ate la!", LANG_UNIVERSAL);
            LOG_INFO("modules", "GUILDHOUSE: GuildId: '{}' comprou uma Casa da Guilda.", player->GetGuildId());

            SpawnStarterPortal(player);
            SpawnButlerNPC(player);
            CloseGossipMenuFor(player);
        }

        return true;
    }

    uint32 GetGuildPhase(Player* player)
    {
        return player->GetGuildId() + 10;
    }

    bool RemoveGuildHouse(Player* player)
    {

        uint32 guildPhase = GetGuildPhase(player);
        QueryResult CreatureResult;
        QueryResult GameobjResult;
        Map *map = sMapMgr->FindMap(1, 0);
        // Lets find all of the gameobjects to be removed
        GameobjResult = WorldDatabase.Query("SELECT `guid` FROM `gameobject` WHERE `map` = 1 AND `phaseMask` = '{}'", guildPhase);
        // Lets find all of the creatures to be removed
        CreatureResult = WorldDatabase.Query("SELECT `guid` FROM `creature` WHERE `map` = 1 AND `phaseMask` = '{}'", guildPhase);

        // Remove creatures from the deleted guild house map
        if (CreatureResult)
        {
            do
            {
                Field* fields = CreatureResult->Fetch();
                uint32 lowguid = fields[0].Get<uint32>();
                if (CreatureData const* cr_data = sObjectMgr->GetCreatureData(lowguid))
                {
                    if (Creature* creature = map->GetCreature(ObjectGuid::Create<HighGuid::Unit>(cr_data->id1, lowguid)))
                    {
                        creature->CombatStop();
                        creature->DeleteFromDB();
                        creature->AddObjectToRemoveList();
                    }
                }
            } while (CreatureResult->NextRow());
        }

        // Remove gameobjects from the deleted guild house map
        if (GameobjResult)
        {
            do
            {
                Field* fields = GameobjResult->Fetch();
                uint32 lowguid = fields[0].Get<uint32>();
                if (GameObjectData const* go_data = sObjectMgr->GetGameObjectData(lowguid))
                {
                    if (GameObject* gobject = map->GetGameObject(ObjectGuid::Create<HighGuid::GameObject>(go_data->id, lowguid)))
                    {
                        gobject->SetRespawnTime(0);
                        gobject->Delete();
                        gobject->DeleteFromDB();
                        gobject->CleanupsBeforeDelete();
                        // delete gobject;
                    }
                }

            } while (GameobjResult->NextRow());
        }

        // Delete actual guild_house data from characters database
        CharacterDatabase.Execute("DELETE FROM `guild_house` WHERE `guild`={}", player->GetGuildId());
        CharacterDatabase.Execute("DELETE FROM `guild_house_zoe` WHERE `guild`={}", player->GetGuildId());
        ZoeGuildHouseLog(player, "SELL_HOUSE", 0, 0, 0, 0);

        return true;
    }

    void SpawnStarterPortal(Player* player)
    {

        uint32 entry = 0;
        float posX;
        float posY;
        float posZ;
        float ori;

        Map* map = sMapMgr->FindMap(1, 0);

        if (player->GetTeamId() == TEAM_ALLIANCE)
        {
            // Portal to Stormwind
            entry = GetGameObjectEntry(0);
        }
        else
        {
            // Portal to Orgrimmar
            entry = GetGameObjectEntry(4);
        }

        if (entry == 0)
        {
            LOG_INFO("modules", "Error with SpawnStarterPortal in GuildHouse Module!");
            return;
        }

        QueryResult result = WorldDatabase.Query("SELECT `posX`, `posY`, `posZ`, `orientation` FROM `guild_house_spawns` WHERE `entry`={}", entry);

        if (!result)
        {
            LOG_INFO("modules", "GUILDHOUSE: Unable to find data on portal for entry: {}", entry);
            return;
        }

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
        {
            LOG_INFO("modules", "GUILDHOUSE: objectId IS NULL, should be '{}'", entry);
            return;
        }

        const GameObjectTemplate* objectInfo = sObjectMgr->GetGameObjectTemplate(objectId);

        if (!objectInfo)
        {
            LOG_INFO("modules", "GUILDHOUSE: objectInfo is NULL!");
            return;
        }

        if (objectInfo->displayId && !sGameObjectDisplayInfoStore.LookupEntry(objectInfo->displayId))
        {
            LOG_INFO("modules", "GUILDHOUSE: Unable to find displayId??");
            return;
        }

        GameObject* object = sObjectMgr->IsGameObjectStaticTransport(objectInfo->entry) ? new StaticTransport() : new GameObject();
        ObjectGuid::LowType guidLow = player->GetMap()->GenerateLowGuid<HighGuid::GameObject>();

        if (!object->Create(guidLow, objectInfo->entry, map, GetGuildPhase(player), posX, posY, posZ, ori, G3D::Quat(), 0, GO_STATE_READY))
        {
            delete object;
            LOG_INFO("modules", "GUILDHOUSE: Unable to create object!!");
            return;
        }

        // fill the gameobject data and save to the db
        object->SaveToDB(sMapMgr->FindMap(1, 0)->GetId(), (1 << sMapMgr->FindMap(1, 0)->GetSpawnMode()), GetGuildPhase(player));
        guidLow = object->GetSpawnId();
        // delete the old object and do a clean load from DB with a fresh new GameObject instance.
        // this is required to avoid weird behavior and memory leaks
        delete object;

        object = sObjectMgr->IsGameObjectStaticTransport(objectInfo->entry) ? new StaticTransport() : new GameObject();
        // this will generate a new guid if the object is in an instance
        if (!object->LoadGameObjectFromDB(guidLow, sMapMgr->FindMap(1, 0), true))
        {
            delete object;
            return;
        }

        // TODO: is it really necessary to add both the real and DB table guid here ?
        sObjectMgr->AddGameobjectToGrid(guidLow, sObjectMgr->GetGameObjectData(guidLow));
        CloseGossipMenuFor(player);
    }

    void SpawnButlerNPC(Player* player)
    {
        uint32 entry = GetCreatureEntry(1);
        float posX = 16202.185547f;
        float posY = 16255.916992f;
        float posZ = 21.160221f;
        float ori = 6.195375f;

        Map* map = sMapMgr->FindMap(1, 0);
        Creature *creature = new Creature();

        if (!creature->Create(map->GenerateLowGuid<HighGuid::Unit>(), map, player->GetPhaseMaskForSpawn(), entry, 0, posX, posY, posZ, ori))
        {
            delete creature;
            return;
        }
        creature->SaveToDB(map->GetId(), (1 << map->GetSpawnMode()), GetGuildPhase(player));
        uint32 lowguid = creature->GetSpawnId();

        creature->CleanupsBeforeDelete();
        delete creature;
        creature = new Creature();
        if (!creature->LoadCreatureFromDB(lowguid, map))
        {
            delete creature;
            return;
        }

        sObjectMgr->AddCreatureToGrid(lowguid, sObjectMgr->GetCreatureData(lowguid));
        return;
    }

    bool BuyGuildHouse(Guild* guild, Player* player, Creature* creature)
    {
        QueryResult result = CharacterDatabase.Query("SELECT `id`, `guild` FROM `guild_house` WHERE `guild`={}", guild->GetId());

        if (result)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("Sua guilda ja possui uma Casa da Guilda.");
            CloseGossipMenuFor(player);
            return false;
        }

        ClearGossipMenuFor(player);
        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Ilha GM", GOSSIP_SENDER_MAIN, 100, "Buy Guild House on Ilha GM?", sConfigMgr->GetOption<int32>("CostGuildHouse", 10000000), false);
        // Removing this tease for now, as right now the phasing code is specific go Ilha GM, so it's not a simple thing to add new areas yet.
        // AddGossipItemFor(player, GOSSIP_ICON_CHAT, " ----- More to Come ----", GOSSIP_SENDER_MAIN, 4);
        SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
        return true;
    }

    void TeleportGuildHouse(Guild* guild, Player* player, Creature* creature)
    {
        GuildData* guildData = player->CustomData.GetDefault<GuildData>("phase");
        QueryResult result = CharacterDatabase.Query("SELECT `phase`, `map`,`positionX`, `positionY`, `positionZ`, `orientation` FROM `guild_house` WHERE `guild`={}", guild->GetId());

        if (!result)
        {
            ClearGossipMenuFor(player);
            if (player->GetGuild()->GetLeaderGUID() == player->GetGUID())
            {
                // Only leader of the guild can buy / sell guild house
                AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Comprar Casa da Guilda", GOSSIP_SENDER_MAIN, 2);
                AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Vender Casa da Guilda", GOSSIP_SENDER_MAIN, 3, "Tem certeza que deseja vender sua Casa da Guilda?", 0, false);
                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "Aumentar Nivel da Casa", GOSSIP_SENDER_MAIN, 7, "Deseja aumentar o nivel da Casa da Guilda?", 0, false);
            }

            AddGossipItemFor(player, GOSSIP_ICON_TABARD, "Teleportar para Casa da Guilda", GOSSIP_SENDER_MAIN, 1);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Informacoes da Guilda", GOSSIP_SENDER_MAIN, 6);
            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "Fechar", GOSSIP_SENDER_MAIN, 5);
            SendGossipMenuFor(player, DEFAULT_GOSSIP_MESSAGE, creature->GetGUID());
            ChatHandler(player->GetSession()).PSendSysMessage("Sua guilda nao possui uma Casa da Guilda");
            return;
        }

        do
        {

            Field* fields = result->Fetch();
            guildData->phase = fields[0].Get<uint32>();
            uint32 map = fields[1].Get<uint32>();
            guildData->posX = fields[2].Get<float>();
            guildData->posY = fields[3].Get<float>();
            guildData->posZ = fields[4].Get<float>();
            guildData->ori = fields[5].Get<float>();

            player->TeleportTo(map, guildData->posX, guildData->posY, guildData->posZ, guildData->ori);

        } while (result->NextRow());
    }
};

class GuildHousePlayerScript : public PlayerScript
{
public:
    GuildHousePlayerScript() : PlayerScript("GuildHousePlayerScript") {}

    void OnPlayerLogin(Player* player)
    {
        CheckPlayer(player);
    }

    void OnPlayerUpdateZone(Player* player, uint32 newZone, uint32 /*newArea*/)
    {
        if (newZone == 876)
            CheckPlayer(player);
        else
            player->SetPhaseMask(GetNormalPhase(player), true);
    }

    bool OnPlayerBeforeTeleport(Player* player, uint32 mapid, float x, float y, float z, float orientation, uint32 options, Unit* target)
    {
        (void)mapid;
        (void)x;
        (void)y;
        (void)z;
        (void)orientation;
        (void)options;
        (void)target;

        if (player->GetZoneId() == 876 && player->GetAreaId() == 876) // Ilha GM
        {
            // Remove the rested state when teleporting from the guild house
            player->RemoveRestState();
        }

        return true;
    }

    uint32 GetNormalPhase(Player* player) const
    {
        if (player->IsGameMaster())
            return PHASEMASK_ANYWHERE;

        uint32 phase = player->GetPhaseByAuras();
        if (!phase)
            return PHASEMASK_NORMAL;
        else
            return phase;
    }

    void CheckPlayer(Player* player)
    {
        GuildData* guildData = player->CustomData.GetDefault<GuildData>("phase");
        QueryResult result = CharacterDatabase.Query("SELECT `id`, `guild`, `phase`, `map`,`positionX`, `positionY`, `positionZ`, `orientation` FROM guild_house WHERE `guild` = {}", player->GetGuildId());

        if (result)
        {
            do
            {
                // commented out due to travis, but keeping for future expansion into other areas
                Field *fields = result->Fetch();
                // uint32 id = fields[0].Get<uint32>();        // fix for travis
                // uint32 guild = fields[1].Get<uint32>();     // fix for travis
                guildData->phase = fields[2].Get<uint32>();
                // uint32 map = fields[3].Get<uint32>();       // fix for travis
                // guildData->posX = fields[4].Get<float>();   // fix for travis
                // guildData->posY = fields[5].Get<float>();   // fix for travis
                // guildData->posZ = fields[6].Get<float>();   // fix for travis
                // guildData->ori = fields[7].Get<float>();   // fix for travis

            } while (result->NextRow());
        }

        if (player->GetZoneId() == 876 && player->GetAreaId() == 876) // Ilha GM
        {
            // Set the guild house as a rested area
            player->SetRestState(0);

            // If player is not in a guild he doesnt have a guild house teleport away
            // TODO: What if they are in a guild, but somehow are in the wrong phaseMask and seeing someone else's area?

            if (!result || !player->GetGuild())
            {
                ChatHandler(player->GetSession()).PSendSysMessage("Sua guilda nao possui uma Casa da Guilda.");
                teleportToDefault(player);
                return;
            }

            player->SetPhaseMask(guildData->phase, true);
        }
        else
            player->SetPhaseMask(GetNormalPhase(player), true);
    }

    void teleportToDefault(Player* player)
    {
        if (player->GetTeamId() == TEAM_ALLIANCE)
            player->TeleportTo(0, -8833.379883f, 628.627991f, 94.006599f, 1.0f);
        else
            player->TeleportTo(1, 1486.048340f, -4415.140625f, 24.187496f, 0.13f);
    }
};

using namespace Acore::ChatCommands;

class GuildHouseCommand : public CommandScript
{
public:
    GuildHouseCommand() : CommandScript("GuildHouseCommand") {}

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable GuildHouseCommandTable =
        {
            {"teleport", HandleGuildHouseTeleCommand, SEC_PLAYER, Console::Yes},
            {"butler", HandleSpawnButlerCommand, SEC_PLAYER, Console::Yes},
            {"info", HandleGuildHouseInfoCommand, SEC_PLAYER, Console::Yes},
            {"reset", HandleGuildHouseResetCommand, SEC_GAMEMASTER, Console::Yes},
            {"upgrade", HandleGuildHouseUpgradeCommand, SEC_PLAYER, Console::Yes},
        };

        static ChatCommandTable GuildHouseCommandBaseTable =
        {
            {"guildhouse", GuildHouseCommandTable},
            {"gh", GuildHouseCommandTable}
        };

        return GuildHouseCommandBaseTable;
    }

    static uint32 GetGuildPhase(Player* player)
    {
        return player->GetGuildId() + 10;
    }


    static bool HandleGuildHouseInfoCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player || !player->GetGuild())
        {
            handler->SendSysMessage("Voce nao esta em uma guilda.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        QueryResult house = CharacterDatabase.Query("SELECT `id` FROM `guild_house` WHERE `guild`={}", player->GetGuildId());
        if (!house)
        {
            handler->SendSysMessage("Sua guilda nao possui uma Casa da Guilda.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        ZoeSendGuildHouseInfo(player, handler);
        return true;
    }

    static bool HandleGuildHouseUpgradeCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player || !player->GetGuild())
        {
            handler->SendSysMessage("Voce nao esta em uma guilda.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!ZoeIsGuildOfficerEnough(player, sConfigMgr->GetOption<int32>("GuildHouseSellRank", 0)))
        {
            handler->SendSysMessage("Voce nao tem permissao para evoluir a Casa da Guilda.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (ZoeGuildHouseBlockCombat(player))
        {
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!CharacterDatabase.Query("SELECT `id` FROM `guild_house` WHERE `guild`={}", player->GetGuildId()))
        {
            handler->SendSysMessage("Sua guilda nao possui uma Casa da Guilda.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 currentLevel = ZoeGuildHouseLevel(player);
        uint32 nextLevel = currentLevel + 1;
        uint32 maxLevel = sConfigMgr->GetOption<uint32>("GuildHouse.Level.Max", 3);

        if (nextLevel > maxLevel)
        {
            handler->SendSysMessage("Sua Casa da Guilda ja esta no level maximo.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!ZoePayHouseUpgrade(player, nextLevel))
        {
            handler->SetSentErrorMessage(true);
            return false;
        }

        handler->PSendSysMessage("Casa da Guilda evoluida para level {}.", nextLevel);
        player->GetGuild()->BroadcastToGuild(player->GetSession(), false, "A Casa da Guilda foi evoluida!", LANG_UNIVERSAL);
        return true;
    }

    static bool HandleGuildHouseResetCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player || !player->GetGuild())
        {
            handler->SendSysMessage("Voce nao esta em uma guilda.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (!ZoeIsGuildOfficerEnough(player, sConfigMgr->GetOption<int32>("GuildHouseSellRank", 0)))
        {
            handler->SendSysMessage("Voce nao tem permissao para resetar a Casa da Guilda.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        uint32 guildPhase = ZoeGuildPhase(player);
        WorldDatabase.Execute("DELETE FROM `creature` WHERE `map`=1 AND `phaseMask`={}", guildPhase);
        WorldDatabase.Execute("DELETE FROM `gameobject` WHERE `map`=1 AND `phaseMask`={}", guildPhase);
        ZoeGuildHouseLog(player, "RESET_SPAWNS", 0, 0, 0, 0);

        handler->SendSysMessage("Spawns da Casa da Guilda removidos do banco. Reinicie o worldserver ou recarregue o mapa para limpar objetos carregados.");
        return true;
    }

    static bool HandleSpawnButlerCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!ZoeGuildHouseEnabled())
        {
            handler->SendSysMessage("O sistema de Casa da Guilda esta desativado.");
            handler->SetSentErrorMessage(true);
            return false;
        }
        if (ZoeGuildHouseBlockCombat(player))
        {
            handler->SetSentErrorMessage(true);
            return false;
        }
        Map* map = player->GetMap();

        if (!player->GetGuild() || (player->GetGuild()->GetLeaderGUID() != player->GetGUID()))
        {
            handler->SendSysMessage("Voce precisa ser o Guild Master para usar este comando!");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (player->GetAreaId() != 876)
        {
            handler->SendSysMessage("Voce precisa estar na sua Casa da Guilda para usar este comando!");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (player->FindNearestCreature(GetCreatureEntry(1), VISIBLE_RANGE, true))
        {
            handler->SendSysMessage("Voce ja possui o Mordomo da Casa da Guilda!");
            handler->SetSentErrorMessage(true);
            return false;
        }

        float posX = 16202.185547f;
        float posY = 16255.916992f;
        float posZ = 21.160221f;
        float ori = 6.195375f;

        Creature* creature = new Creature();
        if (!creature->Create(map->GenerateLowGuid<HighGuid::Unit>(), map, GetGuildPhase(player), GetCreatureEntry(1), 0, posX, posY, posZ, ori))
        {
            handler->SendSysMessage("Voce ja possui o Mordomo da Casa da Guilda!");
            handler->SetSentErrorMessage(true);
            delete creature;
            return false;
        }
        creature->SaveToDB(player->GetMapId(), (1 << player->GetMap()->GetSpawnMode()), GetGuildPhase(player));
        uint32 lowguid = creature->GetSpawnId();

        creature->CleanupsBeforeDelete();
        delete creature;
        creature = new Creature();
        if (!creature->LoadCreatureFromDB(lowguid, player->GetMap()))
        {
            handler->SendSysMessage("Algo deu errado ao adicionar o Mordomo.");
            handler->SetSentErrorMessage(true);
            delete creature;
            return false;
        }

        sObjectMgr->AddCreatureToGrid(lowguid, sObjectMgr->GetCreatureData(lowguid));
        return true;
    }

    static bool HandleGuildHouseTeleCommand(ChatHandler* handler)
    {
        Player* player = handler->GetSession()->GetPlayer();

        if (!player)
            return false;

        if (!ZoeGuildHouseEnabled())
        {
            handler->SendSysMessage("O sistema de Casa da Guilda esta desativado.");
            handler->SetSentErrorMessage(true);
            return false;
        }

        if (player->IsInCombat())
        {
            handler->SendSysMessage("Voce nao pode usar este comando em combate!");
            handler->SetSentErrorMessage(true);
            return false;
        }

        GuildData* guildData = player->CustomData.GetDefault<GuildData>("phase");
        QueryResult result = CharacterDatabase.Query("SELECT `id`, `guild`, `phase`, `map`,`positionX`, `positionY`, `positionZ`, `orientation` FROM `guild_house` WHERE `guild`={}", player->GetGuildId());

        if (!result)
        {
            handler->SendSysMessage("Sua guilda nao possui uma Casa da Guilda!");
            handler->SetSentErrorMessage(true);
            return false;
        }

        do
        {
            Field* fields = result->Fetch();
            // uint32 id = fields[0].Get<uint32>();        // fix for travis
            // uint32 guild = fields[1].Get<uint32>();     // fix for travis
            guildData->phase = fields[2].Get<uint32>();
            uint32 map = fields[3].Get<uint32>();
            guildData->posX = fields[4].Get<float>();
            guildData->posY = fields[5].Get<float>();
            guildData->posZ = fields[6].Get<float>();
            guildData->ori = fields[7].Get<float>();

            player->TeleportTo(map, guildData->posX, guildData->posY, guildData->posZ, guildData->ori);

        } while (result->NextRow());

        return true;
    }
};

class GuildHouseGlobal : public GlobalScript
{
public:
    GuildHouseGlobal() : GlobalScript("GuildHouseGlobal") {}

    void OnBeforeWorldObjectSetPhaseMask(WorldObject const* worldObject, uint32 & /*oldPhaseMask*/, uint32 & /*newPhaseMask*/, bool &useCombinedPhases, bool & /*update*/) override
    {
        if (worldObject->GetZoneId() == 876)
            useCombinedPhases = false;
        else
            useCombinedPhases = true;
    }
};

void AddGuildHouseScripts()
{
    new GuildHelper();
    new GuildHouseSeller();
    new GuildHousePlayerScript();
    new GuildHouseCommand();
    new GuildHouseGlobal();
}
