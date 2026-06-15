/*
 * This file is part of the AzerothCore Project.
 * ZoeCore V1 PT-BR Multi Alert.
 */

#ifndef _SERVER_AUTO_SHUTDOWN_H_
#define _SERVER_AUTO_SHUTDOWN_H_

#include "Common.h"

class ServerAutoShutdown
{
public:
    static ServerAutoShutdown* instance();

    void Init();
    void OnUpdate(uint32 diff);
    void StartPersistentGameEvents();

private:
    bool _isEnableModule = false;
};

#define sSAS ServerAutoShutdown::instance()

#endif /* _SERVER_AUTO_SHUTDOWN_H_ */
