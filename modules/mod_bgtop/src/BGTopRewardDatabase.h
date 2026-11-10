#ifndef ZOE_BGTOP_REWARD_DATABASE_H
#define ZOE_BGTOP_REWARD_DATABASE_H

#include "DatabaseEnv.h"
#include "ModuleDatabasePool.h"
#include "MySQLPreparedStatement.h"

namespace BGTopRewards
{
    enum Statements : uint32
    {
        SELECT_MATCH = MAX_CHARACTERDATABASE_STATEMENTS,
        INSERT_CLAIM,
        UPSERT_TOTAL,
        UPSERT_WEEKLY,
        UPSERT_MONTHLY,
        UPSERT_SUMMARY,
        MAX_STATEMENTS
    };

    class Connection : public CharacterDatabaseConnection
    {
    public:
        using CharacterDatabaseConnection::CharacterDatabaseConnection;

        void DoPrepareStatements() override
        {
            CharacterDatabaseConnection::DoPrepareStatements();
            m_stmts.resize(MAX_STATEMENTS);
            PrepareStatement(SELECT_MATCH,
                "SELECT b.type, b.winner_faction FROM pvpstats_battlegrounds b LEFT JOIN "
                "zoecore_bgtop_processed z ON z.battleground_id=b.id WHERE b.id=? AND z.battleground_id IS "
                "NULL",
                CONNECTION_SYNCH);
            PrepareStatement(INSERT_CLAIM,
                "INSERT INTO zoecore_bgtop_processed (battleground_id, processed_at) VALUES (?, "
                "UNIX_TIMESTAMP())",
                CONNECTION_ASYNC);
            PrepareStatement(UPSERT_TOTAL,
                "INSERT INTO `zoecore_bgtop_stats_total` (`guid`, `player_name`, `kills`, `honorable_kills`, "
                "`deaths`, `damage`, `healing`, `honor`, `bgs_played`, `updated_at`) SELECT "
                "p.`character_guid`, COALESCE(c.`name`, 'Nenhum'), p.`score_killing_blows`, "
                "p.`score_honorable_kills`, p.`score_deaths`, p.`score_damage_done`, p.`score_healing_done`, "
                "p.`score_bonus_honor`, 1, UNIX_TIMESTAMP() FROM `pvpstats_players` p LEFT JOIN `characters` "
                "c ON c.`guid`=p.`character_guid` WHERE p.`battleground_id`=? ON DUPLICATE KEY UPDATE "
                "`player_name`=VALUES(`player_name`), `kills`=`kills`+VALUES(`kills`), "
                "`honorable_kills`=`honorable_kills`+VALUES(`honorable_kills`), "
                "`deaths`=`deaths`+VALUES(`deaths`), `damage`=`damage`+VALUES(`damage`), "
                "`healing`=`healing`+VALUES(`healing`), `honor`=`honor`+VALUES(`honor`), "
                "`bgs_played`=`bgs_played`+1, `updated_at`=UNIX_TIMESTAMP()",
                CONNECTION_ASYNC);
            PrepareStatement(UPSERT_WEEKLY,
                "INSERT INTO `zoecore_bgtop_stats_weekly` (`guid`, `week_year`, `week_number`, `player_name`,"
                " `kills`, `honorable_kills`, `deaths`, `damage`, `healing`, `honor`, `bgs_played`, "
                "`updated_at`) SELECT p.`character_guid`, YEAR(CURDATE()), WEEK(CURDATE(), 3), "
                "COALESCE(c.`name`, 'Nenhum'), p.`score_killing_blows`, p.`score_honorable_kills`, "
                "p.`score_deaths`, p.`score_damage_done`, p.`score_healing_done`, p.`score_bonus_honor`, 1, "
                "UNIX_TIMESTAMP() FROM `pvpstats_players` p LEFT JOIN `characters` c ON "
                "c.`guid`=p.`character_guid` WHERE p.`battleground_id`=? ON DUPLICATE KEY UPDATE "
                "`player_name`=VALUES(`player_name`), `kills`=`kills`+VALUES(`kills`), "
                "`honorable_kills`=`honorable_kills`+VALUES(`honorable_kills`), "
                "`deaths`=`deaths`+VALUES(`deaths`), `damage`=`damage`+VALUES(`damage`), "
                "`healing`=`healing`+VALUES(`healing`), `honor`=`honor`+VALUES(`honor`), "
                "`bgs_played`=`bgs_played`+1, `updated_at`=UNIX_TIMESTAMP()",
                CONNECTION_ASYNC);
            PrepareStatement(UPSERT_MONTHLY,
                "INSERT INTO `zoecore_bgtop_stats_monthly` (`guid`, `month_year`, `month_number`, "
                "`player_name`, `kills`, `honorable_kills`, `deaths`, `damage`, `healing`, `honor`, "
                "`bgs_played`, `updated_at`) SELECT p.`character_guid`, YEAR(CURDATE()), MONTH(CURDATE()), "
                "COALESCE(c.`name`, 'Nenhum'), p.`score_killing_blows`, p.`score_honorable_kills`, "
                "p.`score_deaths`, p.`score_damage_done`, p.`score_healing_done`, p.`score_bonus_honor`, 1, "
                "UNIX_TIMESTAMP() FROM `pvpstats_players` p LEFT JOIN `characters` c ON "
                "c.`guid`=p.`character_guid` WHERE p.`battleground_id`=? ON DUPLICATE KEY UPDATE "
                "`player_name`=VALUES(`player_name`), `kills`=`kills`+VALUES(`kills`), "
                "`honorable_kills`=`honorable_kills`+VALUES(`honorable_kills`), "
                "`deaths`=`deaths`+VALUES(`deaths`), `damage`=`damage`+VALUES(`damage`), "
                "`healing`=`healing`+VALUES(`healing`), `honor`=`honor`+VALUES(`honor`), "
                "`bgs_played`=`bgs_played`+1, `updated_at`=UNIX_TIMESTAMP()",
                CONNECTION_ASYNC);
            PrepareStatement(UPSERT_SUMMARY,
                "REPLACE INTO `zoecore_bgtop_match_summary` (`battleground_id`, `bg_type`, `bg_name`, "
                "`winner_faction`, `top_kill_name`, `top_kill_value`, `top_damage_name`, `top_damage_value`, "
                "`top_heal_name`, `top_heal_value`, `top_hk_name`, `top_hk_value`, `top_honor_name`, "
                "`top_honor_value`, `top_deaths_name`, `top_deaths_value`, `created_at`) VALUES (?, ?, ?, ?, "
                "?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, UNIX_TIMESTAMP())",
                CONNECTION_ASYNC);
        }
    };

    class Pool : public ModuleDatabasePool
    {
    public:
        CharacterDatabasePreparedStatement* Statement(Statements index)
        {
            return new CharacterDatabasePreparedStatement(index, GetPreparedStatementParamCount(index));
        }

    protected:
        MySQLConnection* CreateConnection(MySQLConnectionInfo& info) override { return new Connection(info); }
        MySQLConnection* CreateAsyncConnection(ProducerConsumerQueue<SQLOperation*>* queue,
            MySQLConnectionInfo& info) override { return new Connection(queue, info); }
    };
}

#endif
