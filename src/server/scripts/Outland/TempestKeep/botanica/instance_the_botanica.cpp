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
#include "GameTime.h"
#include "InstanceMapScript.h"
#include "InstanceScript.h"
#include "Item.h"
#include "Mail.h"
#include "Player.h"
#include "SpellScript.h"
#include "SpellScriptLoader.h"
#include "the_botanica.h"
#include <array>

namespace
{
    constexpr uint32 ITEM_EMBLEM_OF_CRUEL_CONQUEST = 45229;
    constexpr uint32 NPC_THE_POSTMASTER = 34337;

    constexpr std::array<uint32, MAX_ENCOUNTER> EMBLEM_REWARDS =
    {
        2, // Commander Sarannis
        2, // High Botanist Freywinn
        3, // Thorngrin the Tender
        4, // Laj
        6  // Warp Splinter
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

class instance_the_botanica : public InstanceMapScript
{
public:
    instance_the_botanica() : InstanceMapScript("instance_the_botanica", MAP_TEMPEST_KEEP_THE_BOTANICA) {}

    struct instance_the_botanica_InstanceMapScript : public InstanceScript
    {
        instance_the_botanica_InstanceMapScript(Map* map) : InstanceScript(map)
        {
            SetHeaders(DataHeader);
            SetBossNumber(MAX_ENCOUNTER);
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
        return new instance_the_botanica_InstanceMapScript(map);
    }
};

class spell_botanica_call_of_the_falcon_aura : public AuraScript
{
    PrepareAuraScript(spell_botanica_call_of_the_falcon_aura);

    bool Load() override
    {
        _falconSet.clear();
        return true;
    }

    void HandleEffectApply(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        std::list<Creature*> creatureList;
        GetUnitOwner()->GetCreaturesWithEntryInRange(creatureList, 80.0f, NPC_BLOODFALCON);
        for (std::list<Creature*>::const_iterator itr = creatureList.begin(); itr != creatureList.end(); ++itr)
        {
            (*itr)->AddThreat(GetUnitOwner(), 10000000.0f);
            if ((*itr)->AI())
                (*itr)->AI()->AttackStart(GetUnitOwner());
            _falconSet.insert((*itr)->GetGUID());
        }
    }

    void HandleEffectRemove(AuraEffect const* /*aurEff*/, AuraEffectHandleModes /*mode*/)
    {
        for (ObjectGuid const& guid : _falconSet)
            if (Creature* falcon = ObjectAccessor::GetCreature(*GetUnitOwner(), guid))
            {
                falcon->AddThreat(GetUnitOwner(), -10000000.0f);
            }
    }

    void Register() override
    {
        OnEffectApply += AuraEffectApplyFn(spell_botanica_call_of_the_falcon_aura::HandleEffectApply, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
        OnEffectRemove += AuraEffectRemoveFn(spell_botanica_call_of_the_falcon_aura::HandleEffectRemove, EFFECT_0, SPELL_AURA_DUMMY, AURA_EFFECT_HANDLE_REAL);
    }

private:
    GuidSet _falconSet;
};

class spell_botanica_shift_form_aura : public AuraScript
{
    PrepareAuraScript(spell_botanica_shift_form_aura);

    bool Load() override
    {
        _lastSchool = 0;
        _lastForm = 0;
        _swapTime = 0;
        return true;
    }

    bool CheckProc(ProcEventInfo& eventInfo)
    {
        if (SpellInfo const* spellInfo = eventInfo.GetSpellInfo())
        {
            if ((spellInfo->GetSchoolMask() & _lastSchool) && _swapTime > GameTime::GetGameTime().count())
                return false;

            uint32 form = 0;
            switch (GetFirstSchoolInMask(spellInfo->GetSchoolMask()))
            {
            case SPELL_SCHOOL_FIRE:
                form = SPELL_FIRE_FORM;
                break;
            case SPELL_SCHOOL_FROST:
                form = SPELL_FROST_FORM;
                break;
            case SPELL_SCHOOL_ARCANE:
                form = SPELL_ARCANE_FORM;
                break;
            case SPELL_SCHOOL_SHADOW:
                form = SPELL_SHADOW_FORM;
                break;
            default:
                break;
            }

            if (form)
            {
                _swapTime = GameTime::GetGameTime().count() + 6;
                _lastSchool = spellInfo->GetSchoolMask();

                // Runs from Aura::GetProcEffectMask while the proc engine iterates the owner's
                // applied-aura map. Swapping forms mutates that map (RemoveAura + CastSpell),
                // which invalidates the live iterator now that aura containers are flat_multimaps.
                // Defer the swap to the owner's event queue so it runs outside the proc walk.
                Unit* owner = GetUnitOwner();
                uint32 const oldForm = _lastForm;
                _lastForm = form;
                owner->m_Events.AddEventAtOffset([owner, oldForm, form]()
                    {
                        owner->RemoveAurasDueToSpell(oldForm);
                        owner->CastSpell(owner, form, true);
                    }, 1ms);
            }
        }

        return false;
    }

    void Register() override
    {
        DoCheckProc += AuraCheckProcFn(spell_botanica_shift_form_aura::CheckProc);
    }

private:
    uint32 _lastSchool;
    uint32 _lastForm;
    uint32 _swapTime;
};

void AddSC_instance_the_botanica()
{
    new instance_the_botanica();
    RegisterSpellScript(spell_botanica_call_of_the_falcon_aura);
    RegisterSpellScript(spell_botanica_shift_form_aura);
}
