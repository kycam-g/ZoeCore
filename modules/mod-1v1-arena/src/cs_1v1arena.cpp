#include "Chat.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Tokenize.h"
#include "DatabaseEnv.h"
#include "Config.h"
#include "BattlegroundMgr.h"
#include "CommandScript.h"
#include "ArenaTeamMgr.h"
#include "npc_1v1arena.h"

using namespace Acore::ChatCommands;

class CommandJoin1v1 : public CommandScript
{
public:
    CommandJoin1v1() : CommandScript("CommandJoin1v1") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable command1v1Table =
        {
            { "rated",       HandleQueueArena1v1Rated,           SEC_PLAYER,        Console::No },
            { "unrated",     HandleQueueArena1v1UnRated,         SEC_PLAYER,        Console::No },
            { "stats",       HandleArena1v1Stats,                SEC_PLAYER,        Console::No },
        };

        static ChatCommandTable commandTable =
        {
            { "q1v1", command1v1Table },
        };

        return commandTable;
    }

    static bool HandleQueueArena1v1Rated(ChatHandler* handler, const char* args)
    {
        return HandleQueueArena1v1(handler, args, true);
    }

    static bool HandleQueueArena1v1UnRated(ChatHandler* handler, const char* args)
    {
        return HandleQueueArena1v1(handler, args, false);
    }

    static bool HandleQueueArena1v1(ChatHandler* handler, const char* /*args*/, bool isRated)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sConfigMgr->GetOption<bool>("Arena1v1.EnableCommand", true))
        {
            handler->SendSysMessage("[ZoeCore Arena 1v1] O comando da Arena 1v1 esta desativado.");
            return false;
        }

        if (!sConfigMgr->GetOption<bool>("Arena1v1.Enable", true))
        {
            handler->SendSysMessage("[ZoeCore Arena 1v1] A Arena 1v1 esta desativada.");
            return false;
        }

        if (player->IsInCombat())
        {
            handler->SendSysMessage("[ZoeCore Arena 1v1] Voce nao pode estar em combate.");
            return false;
        }

        if (player->HasAura(26013) &&
            (sConfigMgr->GetOption<bool>("Arena1v1.CastDeserterOnAfk", true) ||
                sConfigMgr->GetOption<bool>("Arena1v1.CastDeserterOnLeave", true)))
        {
            WorldPacket data;
            sBattlegroundMgr->BuildGroupJoinedBattlegroundPacket(
                &data, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
            player->GetSession()->SendPacket(&data);

            handler->PSendSysMessage("[ZoeCore Arena 1v1] Voce nao pode entrar com Desertor ativo!");
            return false;
        }

        uint32 minLevel = sConfigMgr->GetOption<uint32>("Arena1v1.MinLevel", 80);
        if (player->GetLevel() < minLevel)
        {
            handler->PSendSysMessage("[ZoeCore Arena 1v1] Voce precisa ser level {}+ para entrar.", minLevel);
            return false;
        }

        npc_1v1arena Command1v1;
        uint32 arenaSlot = sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3);

        if (isRated)
        {
            if (!player->GetArenaTeamId(arenaSlot))
            {
                uint32 cost = sConfigMgr->GetOption<uint32>("Arena1v1.Costs", 400000);

                if (player->GetMoney() < cost)
                {
                    handler->PSendSysMessage("[ZoeCore Arena 1v1] Voce precisa de {} gold para criar um time Arena 1v1.", cost / GOLD);
                    return false;
                }

                if (!Command1v1.CreateArenateam(player, nullptr))
                    return false;

                player->ModifyMoney(-int32(cost));

                handler->SendSysMessage("[ZoeCore Arena 1v1] Time Arena 1v1 criado com sucesso. Use o comando novamente para entrar na fila rated.");
                return true;
            }
        }

        if (Command1v1.JoinQueueArena(player, nullptr, isRated))
        {
            handler->PSendSysMessage("[ZoeCore Arena 1v1] Voce entrou na fila Arena 1v1 {}.", isRated ? "Rated" : "UnRated");
        }

        return true;
    }

    static bool HandleArena1v1Stats(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sConfigMgr->GetOption<bool>("Arena1v1.Enable", true))
        {
            handler->SendSysMessage("[ZoeCore Arena 1v1] A Arena 1v1 esta desativada.");
            return false;
        }

        uint32 arenaSlot = sConfigMgr->GetOption<uint32>("Arena1v1.ArenaSlotID", 3);
        uint32 teamId = player->GetArenaTeamId(arenaSlot);

        if (!teamId)
        {
            handler->SendSysMessage("[ZoeCore Arena 1v1] Voce nao possui um time Arena 1v1.");
            return false;
        }

        ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(teamId);
        if (!at)
        {
            handler->SendSysMessage("[ZoeCore Arena 1v1] Time Arena 1v1 nao encontrado.");
            return false;
        }

        ArenaTeamStats const& stats = at->GetStats();

        std::stringstream s;
        s << "===== ZoeCore Arena 1v1 =====";
        s << "\nRating: " << stats.Rating;
        s << "\nRank: " << stats.Rank;
        s << "\nJogos da temporada: " << stats.SeasonGames;
        s << "\nVitorias da temporada: " << stats.SeasonWins;
        s << "\nJogos da semana: " << stats.WeekGames;
        s << "\nVitorias da semana: " << stats.WeekWins;

        handler->SendSysMessage(s.str().c_str());

        return true;
    }
};

void AddSC_arena1v1_commandscript()
{
    new CommandJoin1v1();
}
