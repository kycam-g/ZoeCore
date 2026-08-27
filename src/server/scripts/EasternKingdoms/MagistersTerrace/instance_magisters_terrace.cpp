/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "CreatureScript.h"
#include "DatabaseEnv.h"
#include "InstanceMapScript.h"
#include "InstanceScript.h"
#include "Item.h"
#include "Mail.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "magisters_terrace.h"
#include <array>

namespace
{
    constexpr uint32 ITEM_EMBLEM_OF_CRUEL_CONQUEST = 45229;
    constexpr uint32 NPC_THE_POSTMASTER = 34337;

    constexpr std::array<uint32, MAX_ENCOUNTER> EMBLEM_REWARDS =
    {
        3, // Selin Fireheart
        3, // Vexallus
        4, // Priestess Delrissa
        7  // Kael'thas Sunstrider
    };

    void SendEmblemsByMail(Player* player, uint32 count)
    {
        Item* item = Item::CreateItem(ITEM_EMBLEM_OF_CRUEL_CONQUEST, count, player);
        if (!item)
            return;

        CharacterDatabaseTransaction transaction = CharacterDatabase.BeginTransaction();
        item->SaveToDB(transaction);

        MailDraft("Emblemas recuperados",
            "Suas bolsas estavam cheias. Os Emblemas da Conquista Cruel conquistados foram enviados em anexo.")
            .AddItem(item)
            .SendMailTo(transaction, MailReceiver(player), MailSender(MAIL_CREATURE, NPC_THE_POSTMASTER));

        CharacterDatabase.CommitTransaction(transaction);
    }

    void RewardEmblems(Player* player, uint32 count)
    {
        uint32 noSpaceForCount = 0;
        ItemPosCountVec destination;
        InventoryResult result = player->CanStoreNewItem(NULL_BAG, NULL_SLOT, destination,
            ITEM_EMBLEM_OF_CRUEL_CONQUEST, count, &noSpaceForCount);

        if (result != EQUIP_ERR_OK && noSpaceForCount == 0)
            noSpaceForCount = count;

        uint32 storeCount = count - noSpaceForCount;
        if (storeCount > 0 && !destination.empty())
        {
            if (Item* item = player->StoreNewItem(destination, ITEM_EMBLEM_OF_CRUEL_CONQUEST, true))
                player->SendNewItem(item, storeCount, true, false);
            else
                noSpaceForCount += storeCount;
        }

        if (noSpaceForCount > 0)
            SendEmblemsByMail(player, noSpaceForCount);
    }
}

ObjectData const creatureData[] =
{
    { NPC_DELRISSA,        DATA_DELRISSA         },
    { NPC_KAEL_THAS,       DATA_KAELTHAS         },
    { NPC_KALECGOS,        DATA_KALECGOS         },
    { 0,                   0                     }
};

ObjectData const gameobjectData[] =
{
    { GO_ESCAPE_ORB, DATA_ESCAPE_ORB },
    { 0,             0,              }
};

ObjectData const summonerData[] =
{
    { NPC_PHOENIX,       DATA_KAELTHAS },
    { NPC_PHOENIX_EGG,   DATA_KAELTHAS },
    { 0,                 0             }
};

DoorData const doorData[] =
{
    { GO_SELIN_DOOR,           DATA_SELIN_FIREHEART, DOOR_TYPE_PASSAGE },
    { GO_SELIN_ENCOUNTER_DOOR, DATA_SELIN_FIREHEART, DOOR_TYPE_ROOM    },
    { GO_VEXALLUS_DOOR,        DATA_VEXALLUS,        DOOR_TYPE_PASSAGE },
    { GO_DELRISSA_DOOR,        DATA_DELRISSA,        DOOR_TYPE_PASSAGE },
    { GO_KAEL_DOOR,            DATA_KAELTHAS,        DOOR_TYPE_ROOM    },
    { 0,                       0,                    DOOR_TYPE_ROOM    } // END
};

BossBoundaryData const boundaries =
{
    { DATA_KAELTHAS, new RectangleBoundary(118.64f, 178.63f, 125.69f, 210.70f) }
};

Position const KalecgosSpawnPos = { 164.3747f, -397.1197f, 2.151798f, 1.66219f };

class instance_magisters_terrace : public InstanceMapScript
{
public:
    instance_magisters_terrace() : InstanceMapScript("instance_magisters_terrace", MAP_MAGISTERS_TERRACE) {}

    struct instance_magisters_terrace_InstanceMapScript : public InstanceScript
    {
        instance_magisters_terrace_InstanceMapScript(Map* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(MAX_ENCOUNTER);
            LoadBossBoundaries(boundaries);
            SetPersistentDataCount(MAX_PERSISTENT_DATA);
            LoadObjectData(creatureData, gameobjectData);
            LoadDoorData(doorData);
            LoadSummonData(summonerData);
        }

        void ProcessEvent(WorldObject* /*obj*/, uint32 eventId) override
        {
            if (eventId == EVENT_SPAWN_KALECGOS)
                if (!GetCreature(DATA_KALECGOS) && !scheduler.IsGroupScheduled(DATA_KALECGOS))
                {
                    scheduler.Schedule(1min, 1min, DATA_KALECGOS, [this](TaskContext)
                        {
                            if (Creature* kalecgos = instance->SummonCreature(NPC_KALECGOS, KalecgosSpawnPos))
                            {
                                kalecgos->GetMotionMaster()->MoveWaypoint(PATH_KALECGOS_FLIGHT, false);
                                kalecgos->AI()->Talk(SAY_KALECGOS_SPAWN);
                            }
                        });
                }
        }

        bool SetBossState(uint32 id, EncounterState state) override
        {
            if (!InstanceScript::SetBossState(id, state))
                return false;

            if (state != DONE || !instance->IsHeroic() || id >= EMBLEM_REWARDS.size())
                return true;

            uint32 reward = EMBLEM_REWARDS[id];
            instance->DoForAllPlayers([reward](Player* player)
                {
                    if (!player->IsGameMaster())
                        RewardEmblems(player, reward);
                });

            return true;
        }
    };

    InstanceScript* GetInstanceScript(InstanceMap* map) const override
    {
        return new instance_magisters_terrace_InstanceMapScript(map);
    }
};

void AddSC_instance_magisters_terrace()
{
    new instance_magisters_terrace();
}
