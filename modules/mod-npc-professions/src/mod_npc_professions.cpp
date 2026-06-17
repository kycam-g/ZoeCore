/*
 * ZoeCore NPC Professions
 * Atualizado para core atual com acesso VIP configurável.
 */

#include "Chat.h"
#include "Config.h"
#include "Creature.h"
#include "DBCStores.h"
#include "GossipDef.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "ScriptedGossip.h"
#include "SpellInfo.h"
#include "SpellMgr.h"
#include "World.h"

#include <array>
#include <string>

namespace
{
    struct ProfessionData
    {
        uint32 MenuAction;
        uint32 LearnAction;
        uint32 VendorAction;
        uint32 SkillId;
        uint32 VendorEntry;
        bool Primary;
        char const* Name;
        char const* Icon;
    };

    constexpr uint32 ACTION_BACK = 401;

    std::array<ProfessionData, 13> const Professions =
    {{
        {100, 200, 300, SKILL_ALCHEMY,        200038, true,  "Alchemy",        "Trade_Alchemy"},
        {101, 201, 301, SKILL_BLACKSMITHING, 200039, true,  "Blacksmithing",  "Trade_BlackSmithing"},
        {102, 202, 302, SKILL_LEATHERWORKING,200040, true,  "Leatherworking", "Trade_LeatherWorking"},
        {103, 203, 303, SKILL_TAILORING,     200041, true,  "Tailoring",      "Trade_Tailoring"},
        {104, 204, 304, SKILL_ENGINEERING,   200042, true,  "Engineering",    "Inv_Misc_Wrench_01"},
        {105, 205, 305, SKILL_ENCHANTING,    200043, true,  "Enchanting",     "Trade_Engraving"},
        {106, 206, 306, SKILL_JEWELCRAFTING, 200044, true,  "Jewelcrafting",  "INV_Misc_Gem_02"},
        {107, 207, 307, SKILL_INSCRIPTION,   200045, true,  "Inscription",    "INV_Inscription_Tradeskill01"},
        {108, 208, 0,   SKILL_HERBALISM,     0,      true,  "Herbalism",      "Trade_Herbalism"},
        {109, 209, 0,   SKILL_SKINNING,      0,      true,  "Skinning",       "INV_Misc_Pelt_Wolf_01"},
        {110, 210, 0,   SKILL_MINING,        0,      true,  "Mining",         "Trade_Mining"},
        {111, 211, 311, SKILL_COOKING,       200046, false, "Cooking",        "INV_Misc_Food_15"},
        {112, 212, 312, SKILL_FIRST_AID,     200047, false, "First Aid",      "Spell_Holy_SealOfSacrifice"}
    }};

    bool Enabled()
    {
        return sConfigMgr->GetOption<bool>("ProfessionsNPC.Enable", true);
    }

    bool IsGMAllowed(Player* player, std::string const& prefix)
    {
        if (!player || !player->GetSession())
            return false;

        return sConfigMgr->GetOption<bool>(prefix + "AllowGM", true) && player->GetSession()->IsGMAccount();
    }

    bool HasVipItem(Player* player, std::string const& prefix)
    {
        if (!player)
            return false;

        uint32 item1 = sConfigMgr->GetOption<uint32>(prefix + "Vip.ItemEntry", 33564);
        uint32 item2 = sConfigMgr->GetOption<uint32>(prefix + "Vip.ItemEntry2", 33565);
        bool includeBank = sConfigMgr->GetOption<bool>(prefix + "Vip.IncludeBank", true);

        if (item1 && player->HasItemCount(item1, 1, includeBank))
            return true;

        if (item2 && player->HasItemCount(item2, 1, includeBank))
            return true;

        return false;
    }

    bool HasAccess(Player* player)
    {
        if (!sConfigMgr->GetOption<bool>("ProfessionsNPC.Access.VipOnly.Enable", false))
            return true;

        if (IsGMAllowed(player, "ProfessionsNPC.Access."))
            return true;

        return HasVipItem(player, "ProfessionsNPC.Access.");
    }

    bool HasVendorAccess(Player* player)
    {
        if (!sConfigMgr->GetOption<bool>("ProfessionsNPC.Vendor.VipOnly.Enable", false))
            return true;

        if (IsGMAllowed(player, "ProfessionsNPC.Vendor."))
            return true;

        return HasVipItem(player, "ProfessionsNPC.Vendor.");
    }

    bool HasPrimaryVip(Player* player)
    {
        if (!player)
            return false;

        uint32 item1 = sConfigMgr->GetOption<uint32>("ProfessionsNPC.Primary.Vip.ItemEntry", 33565);
        uint32 item2 = sConfigMgr->GetOption<uint32>("ProfessionsNPC.Primary.Vip.ItemEntry2", 0);
        bool includeBank = sConfigMgr->GetOption<bool>("ProfessionsNPC.Primary.Vip.IncludeBank", true);

        if (!item1 && !item2)
            return HasVipItem(player, "ProfessionsNPC.Access.");

        if (item1 && player->HasItemCount(item1, 1, includeBank))
            return true;

        if (item2 && player->HasItemCount(item2, 1, includeBank))
            return true;

        return false;
    }

    ProfessionData const* GetProfessionByMenu(uint32 action)
    {
        for (ProfessionData const& prof : Professions)
            if (prof.MenuAction == action)
                return &prof;

        return nullptr;
    }

    ProfessionData const* GetProfessionByLearn(uint32 action)
    {
        for (ProfessionData const& prof : Professions)
            if (prof.LearnAction == action)
                return &prof;

        return nullptr;
    }

    ProfessionData const* GetProfessionByVendor(uint32 action)
    {
        for (ProfessionData const& prof : Professions)
            if (prof.VendorAction && prof.VendorAction == action)
                return &prof;

        return nullptr;
    }

    bool IsSecondary(uint32 skillId)
    {
        return skillId == SKILL_COOKING || skillId == SKILL_FIRST_AID;
    }

    uint32 CountPrimaryProfessions(Player const* player)
    {
        if (!player)
            return 0;

        uint32 count = 0;

        for (ProfessionData const& prof : Professions)
        {
            if (!prof.Primary)
                continue;

            if (player->HasSkill(prof.SkillId))
                ++count;
        }

        return count;
    }

    uint32 GetMaxPrimaryProfessions(Player* player)
    {
        if (HasPrimaryVip(player))
            return sConfigMgr->GetOption<uint32>("ProfessionsNPC.Primary.MaxVip", 3);

        return sConfigMgr->GetOption<uint32>("ProfessionsNPC.Primary.MaxNormal", 2);
    }

    void SendNpcMessage(Player* player, std::string const& text)
    {
        if (!player || !player->GetSession())
            return;

        ChatHandler(player->GetSession()).SendSysMessage(text.c_str());
    }

    void LearnSkillRecipes(Player* player, uint32 skillId)
    {
        if (!player)
            return;

        uint32 classMask = player->getClassMask();

        for (uint32 j = 0; j < sSkillLineAbilityStore.GetNumRows(); ++j)
        {
            SkillLineAbilityEntry const* skillLine = sSkillLineAbilityStore.LookupEntry(j);
            if (!skillLine)
                continue;

            if (skillLine->SkillLine != skillId)
                continue;

            if (skillLine->SupercededBySpell)
                continue;

            if (skillLine->RaceMask != 0)
                continue;

            if (skillLine->ClassMask && (skillLine->ClassMask & classMask) == 0)
                continue;

            SpellInfo const* spellInfo = sSpellMgr->GetSpellInfo(skillLine->Spell);
            if (!spellInfo)
                continue;

            player->learnSpell(skillLine->Spell);
        }
    }

    bool LearnProfession(Player* player, ProfessionData const& prof)
    {
        if (!player)
            return false;

        if (player->HasSkill(prof.SkillId))
        {
            SendNpcMessage(player, sConfigMgr->GetOption<std::string>("ProfessionsNPC.Message.AlreadyKnown", "|cffffcc00[Profissoes]|r Voce ja conhece esta profissao."));
            return true;
        }

        if (prof.Primary)
        {
            uint32 maxPrimary = GetMaxPrimaryProfessions(player);
            if (CountPrimaryProfessions(player) >= maxPrimary)
            {
                SendNpcMessage(player, sConfigMgr->GetOption<std::string>("ProfessionsNPC.Primary.MaxMessage", "|cffff2020[Profissoes]|r Voce ja treinou o maximo de profissoes primarias."));
                return false;
            }
        }

        SkillLineEntry const* skillInfo = sSkillLineStore.LookupEntry(prof.SkillId);
        if (!skillInfo)
            return false;

        LearnSkillRecipes(player, prof.SkillId);

        uint32 maxSkill = sConfigMgr->GetOption<uint32>("ProfessionsNPC.Skill.MaxValue", 450);
        player->SetSkill(prof.SkillId, player->GetSkillStep(prof.SkillId), maxSkill, maxSkill);

        SendNpcMessage(player, sConfigMgr->GetOption<std::string>("ProfessionsNPC.Message.Learned", "|cff00ff00[Profissoes]|r Profissao treinada com sucesso."));
        return true;
    }

    std::string IconText(char const* icon)
    {
        uint32 size = sConfigMgr->GetOption<uint32>("ProfessionsNPC.Menu.IconSize", 30);
        int32 offsetX = sConfigMgr->GetOption<int32>("ProfessionsNPC.Menu.IconOffsetX", -18);
        int32 offsetY = sConfigMgr->GetOption<int32>("ProfessionsNPC.Menu.IconOffsetY", 0);

        return "|TInterface/ICONS/" + std::string(icon) + ":" + std::to_string(size) + ":" + std::to_string(size) + ":" + std::to_string(offsetX) + ":" + std::to_string(offsetY) + "|t";
    }

    void SendMainMenu(Player* player, Creature* creature)
    {
        if (!player || !creature)
            return;

        ClearGossipMenuFor(player);

        if (sConfigMgr->GetOption<bool>("ProfessionsNPC.Menu.ShowPrimary", true))
        {

            for (ProfessionData const& prof : Professions)
            {
                if (!prof.Primary)
                    continue;

                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, IconText(prof.Icon) + " " + prof.Name, GOSSIP_SENDER_MAIN, prof.MenuAction);
            }
        }

        if (sConfigMgr->GetOption<bool>("ProfessionsNPC.Menu.ShowSecondary", true))
        {

            for (ProfessionData const& prof : Professions)
            {
                if (prof.Primary)
                    continue;

                AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, IconText(prof.Icon) + " " + prof.Name, GOSSIP_SENDER_MAIN, prof.MenuAction);
            }
        }

        SendGossipMenuFor(player, sConfigMgr->GetOption<uint32>("ProfessionsNPC.Npc.GossipTextId", 60005), creature->GetGUID());
    }

    void SendProfessionMenu(Player* player, Creature* creature, ProfessionData const& prof)
    {
        if (!player || !creature)
            return;

        ClearGossipMenuFor(player);

        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, IconText("INV_Misc_Book_09") + " Treinar " + prof.Name, GOSSIP_SENDER_MAIN, prof.LearnAction);

        if (sConfigMgr->GetOption<bool>("ProfessionsNPC.Vendor.Enable", true) &&
            sConfigMgr->GetOption<bool>("ProfessionsNPC.Menu.ShowReagentVendor", true) &&
            prof.VendorEntry != 0)
        {
            AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, IconText("INV_Alchemy_CrystalVial") + " Shop Reagents", GOSSIP_SENDER_MAIN, prof.VendorAction);
        }

        AddGossipItemFor(player, GOSSIP_ICON_MONEY_BAG, "|TInterface/ICONS/Achievement_BG_returnXflags_def_WSG:30:30:-18:0|t Voltar", GOSSIP_SENDER_MAIN, ACTION_BACK);
        SendGossipMenuFor(player, sConfigMgr->GetOption<uint32>("ProfessionsNPC.Npc.GossipTextId", 60005), creature->GetGUID());
    }
}

class npc_professions_zoecore : public CreatureScript
{
public:
    npc_professions_zoecore() : CreatureScript("npc_professions_zoecore") { }

    bool OnGossipHello(Player* player, Creature* creature) override
    {
        if (!player || !creature || !Enabled())
            return false;

        if (!HasAccess(player))
        {
            SendNpcMessage(player, sConfigMgr->GetOption<std::string>("ProfessionsNPC.Access.DenyMessage", "|cffff2020[Profissoes]|r Voce precisa do item VIP para usar este NPC."));
            return true;
        }

        SendMainMenu(player, creature);
        return true;
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action) override
    {
        if (!player || !creature || !Enabled())
            return false;

        if (action == ACTION_BACK)
        {
            SendMainMenu(player, creature);
            return true;
        }

        if (ProfessionData const* prof = GetProfessionByMenu(action))
        {
            SendProfessionMenu(player, creature, *prof);
            return true;
        }

        if (ProfessionData const* prof = GetProfessionByLearn(action))
        {
            LearnProfession(player, *prof);
            CloseGossipMenuFor(player);
            return true;
        }

        if (ProfessionData const* prof = GetProfessionByVendor(action))
        {
            if (!sConfigMgr->GetOption<bool>("ProfessionsNPC.Vendor.Enable", true))
            {
                CloseGossipMenuFor(player);
                return true;
            }

            if (!HasVendorAccess(player))
            {
                SendNpcMessage(player, sConfigMgr->GetOption<std::string>("ProfessionsNPC.Vendor.DenyMessage", "|cffff2020[Profissoes]|r Voce precisa do item VIP para abrir o vendor de reagentes."));
                CloseGossipMenuFor(player);
                return true;
            }

            ClearGossipMenuFor(player);
            player->GetSession()->SendListInventory(creature->GetGUID(), prof->VendorEntry);
            return true;
        }

        CloseGossipMenuFor(player);
        return true;
    }
};

void AddSC_ZoeCoreNpcProfessions()
{
    new npc_professions_zoecore();
}
