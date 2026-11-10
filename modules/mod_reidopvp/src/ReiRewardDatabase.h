#ifndef ZOE_REI_REWARD_DATABASE_H
#define ZOE_REI_REWARD_DATABASE_H

#include "DatabaseEnv.h"
#include "ModuleDatabasePool.h"
#include "MySQLPreparedStatement.h"

namespace ReiRewards
{
    enum Statements : uint32
    {
        SELECT_PENDING = MAX_CHARACTERDATABASE_STATEMENTS,
        MARK_DELIVERED,
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
            PrepareStatement(SELECT_PENDING,
                "SELECT id, period, item_entry, item_count, honor, arena FROM zoecore_pvp_king_reward_pending"
                " WHERE guid=? AND delivered=0 ORDER BY id",
                CONNECTION_SYNCH);
            PrepareStatement(MARK_DELIVERED,
                "UPDATE zoecore_pvp_king_reward_pending SET delivered=1, delivered_at=? WHERE id=? AND guid=?",
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
