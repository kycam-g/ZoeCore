/*
 * This file is part of the AzerothCore Project.
 * ZoeCore V1 PT-BR Multi Alert.
 */

#include "Common.h"
#include "Config.h"
#include "Duration.h"
#include "GameEventMgr.h"
#include "Language.h"
#include "Log.h"
#include "ObjectMgr.h"
#include "ServerAutoShutdown.h"
#include "StringConvert.h"
#include "StringFormat.h"
#include "TaskScheduler.h"
#include "Tokenize.h"
#include "Util.h"
#include "World.h"
#include "WorldSessionMgr.h"

#include <string>
#include <string_view>
#include <vector>

namespace
{
    TaskScheduler scheduler;

    time_t GetNextResetTime(time_t time, uint32 restartDays, uint8 restartHour, uint8 restartMinute, uint8 restartSecond)
    {
        tm timeLocal = Acore::Time::TimeBreakdown(time);
        timeLocal.tm_hour = restartHour;
        timeLocal.tm_min = restartMinute;
        timeLocal.tm_sec = restartSecond;

        time_t midnightLocal = mktime(&timeLocal);

        if (restartDays > 1 || midnightLocal <= time)
            midnightLocal += DAY * restartDays;

        return midnightLocal;
    }

    time_t GetNextWeekdayTime(time_t now, int weekday, uint8 restartHour, uint8 restartMinute, uint8 restartSecond)
    {
        tm timeLocal = Acore::Time::TimeBreakdown(now);
        int currentWeekday = timeLocal.tm_wday;
        int daysUntil = (weekday - currentWeekday + 7) % 7;

        if (daysUntil == 0)
        {
            if (timeLocal.tm_hour > restartHour ||
                (timeLocal.tm_hour == restartHour && timeLocal.tm_min > restartMinute) ||
                (timeLocal.tm_hour == restartHour && timeLocal.tm_min == restartMinute && timeLocal.tm_sec >= restartSecond))
            {
                daysUntil = 7;
            }
        }

        timeLocal.tm_mday += daysUntil;
        timeLocal.tm_hour = restartHour;
        timeLocal.tm_min = restartMinute;
        timeLocal.tm_sec = restartSecond;

        return mktime(&timeLocal);
    }

    std::string BuildTimeText(uint32 seconds)
    {
        return Acore::Time::ToTimeString<Seconds>(seconds, TimeOutput::Seconds, TimeFormat::FullText);
    }

    std::string BuildRestartMessage(std::string const& format, uint32 seconds)
    {
        return Acore::StringFormat(format, BuildTimeText(seconds));
    }

    void BroadcastRestartMessage(std::string const& message)
    {
        LOG_INFO("module", "> {}", message);
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, message);
    }

    void ScheduleExtraAnnouncements(uint32 diffToShutdown, uint32 preAnnounceSeconds)
    {
        if (!sConfigMgr->GetOption<bool>("ServerAutoShutdown.MultiAnnounce.Enable", true))
            return;

        std::string announceList = sConfigMgr->GetOption<std::string>("ServerAutoShutdown.MultiAnnounce.Seconds", "1800 900 600 300 60 30 10");
        std::string announceMessageFormat = sConfigMgr->GetOption<std::string>("ServerAutoShutdown.MultiAnnounce.Message", "|cffff2020[ZoeCore]|r Reinicio automatico em |cffffff00{}|r.");

        std::vector<std::string_view> tokens = Acore::Tokenize(announceList, ' ', false);

        for (auto token : tokens)
        {
            if (token.empty())
                continue;

            auto announceSecondsOpt = Acore::StringTo<uint32>(token);
            if (!announceSecondsOpt)
                continue;

            uint32 announceSeconds = *announceSecondsOpt;

            if (announceSeconds == 0)
                continue;

            // O pre-anuncio principal ja agenda o ShutdownServ.
            if (announceSeconds == preAnnounceSeconds)
                continue;

            if (announceSeconds >= diffToShutdown)
                continue;

            uint32 delaySeconds = diffToShutdown - announceSeconds;

            scheduler.Schedule(Seconds(delaySeconds), [announceSeconds, announceMessageFormat](TaskContext /*context*/)
            {
                BroadcastRestartMessage(BuildRestartMessage(announceMessageFormat, announceSeconds));
            });

            if (sConfigMgr->GetOption<bool>("ServerAutoShutdown.Zoe.Log.Enable", true))
            {
                LOG_INFO("module", "> ServerAutoShutdown ZoeCore: Extra announce scheduled at {}", Acore::Time::ToTimeString<Seconds>(announceSeconds));
            }
        }
    }
}

/*static*/ ServerAutoShutdown* ServerAutoShutdown::instance()
{
    static ServerAutoShutdown instance;
    return &instance;
}

void ServerAutoShutdown::Init()
{
    _isEnableModule = sConfigMgr->GetOption<bool>("ServerAutoShutdown.Enabled", false);

    if (!_isEnableModule)
        return;

    std::string configTime = sConfigMgr->GetOption<std::string>("ServerAutoShutdown.Time", "04:00:00");
    auto const& tokens = Acore::Tokenize(configTime, ':', false);

    if (tokens.size() != 3)
    {
        LOG_ERROR("module", "> ServerAutoShutdown: Incorrect time in config option 'ServerAutoShutdown.Time' - '{}'", configTime);
        _isEnableModule = false;
        return;
    }

    auto CheckTime = [tokens](std::initializer_list<uint8> index)
    {
        for (auto const& itr : index)
            if (!Acore::StringTo<uint8>(tokens.at(itr)))
                return false;

        return true;
    };

    if (!CheckTime({ 0, 1, 2 }))
    {
        LOG_ERROR("module", "> ServerAutoShutdown: Incorrect time in config option 'ServerAutoShutdown.Time' - '{}'", configTime);
        _isEnableModule = false;
        return;
    }

    int weekday = sConfigMgr->GetOption<int>("ServerAutoShutdown.Weekday", -1);
    uint32 restartDays = sConfigMgr->GetOption<uint32>("ServerAutoShutdown.EveryDays", 1);
    uint8 restartHour = *Acore::StringTo<uint8>(tokens.at(0));
    uint8 restartMinute = *Acore::StringTo<uint8>(tokens.at(1));
    uint8 restartSecond = *Acore::StringTo<uint8>(tokens.at(2));

    auto nowTime = time(nullptr);
    uint64 nextResetTime = 0;

    if (weekday >= 0 && weekday <= 6)
    {
        nextResetTime = GetNextWeekdayTime(nowTime, weekday, restartHour, restartMinute, restartSecond);
    }
    else
    {
        if (restartDays < 1 || restartDays > 365)
        {
            LOG_ERROR("module", "> ServerAutoShutdown: Incorrect day in config option 'ServerAutoShutdown.EveryDays' - '{}'", restartDays);
            _isEnableModule = false;
            return;
        }

        nextResetTime = GetNextResetTime(nowTime, restartDays, restartHour, restartMinute, restartSecond);
    }

    if (restartDays < 1 || restartDays > 365)
    {
        LOG_ERROR("module", "> ServerAutoShutdown: Incorrect day in config option 'ServerAutoShutdown.EveryDays' - '{}'", restartDays);
        _isEnableModule = false;
    }
    else if (restartHour > 23)
    {
        LOG_ERROR("module", "> ServerAutoShutdown: Incorrect hour in config option 'ServerAutoShutdown.Time' - '{}'", configTime);
        _isEnableModule = false;
    }
    else if (restartMinute >= 60)
    {
        LOG_ERROR("module", "> ServerAutoShutdown: Incorrect minute in config option 'ServerAutoShutdown.Time' - '{}'", configTime);
        _isEnableModule = false;
    }
    else if (restartSecond >= 60)
    {
        LOG_ERROR("module", "> ServerAutoShutdown: Incorrect second in config option 'ServerAutoShutdown.Time' - '{}'", configTime);
        _isEnableModule = false;
    }

    if (!_isEnableModule)
        return;

    uint32 diffToShutdown = nextResetTime - static_cast<uint32>(nowTime);

    if (diffToShutdown < 10)
    {
        LOG_WARN("module", "> ServerAutoShutdown: Next time to shutdown < 10 seconds, Set next period");

        if (weekday >= 0 && weekday <= 6)
            nextResetTime += WEEK;
        else
            nextResetTime += DAY * restartDays;

        diffToShutdown = nextResetTime - static_cast<uint32>(nowTime);
    }

    LOG_INFO("module", " ");
    LOG_INFO("module", "> ServerAutoShutdown ZoeCore: System loading");

    scheduler.CancelAll();
    sWorld->ShutdownCancel();

    LOG_INFO("module", "> ServerAutoShutdown ZoeCore: Next time to shutdown - {}", Acore::Time::TimeToHumanReadable(Seconds(nextResetTime)));
    LOG_INFO("module", "> ServerAutoShutdown ZoeCore: Remaining time to shutdown - {}", Acore::Time::ToTimeString<Seconds>(diffToShutdown));
    LOG_INFO("module", " ");

    uint32 preAnnounceSeconds = sConfigMgr->GetOption<uint32>("ServerAutoShutdown.PreAnnounce.Seconds", HOUR);
    if (preAnnounceSeconds > DAY)
    {
        LOG_ERROR("module", "> ServerAutoShutdown: PreAnnounce.Seconds greater than 1 day ({}). Changed to 1 hour (3600)", preAnnounceSeconds);
        preAnnounceSeconds = HOUR;
    }

    uint32 timeToPreAnnounce = static_cast<uint32>(nextResetTime) - preAnnounceSeconds;
    uint32 diffToPreAnnounce = timeToPreAnnounce - static_cast<uint32>(nowTime);

    if (diffToShutdown < preAnnounceSeconds)
    {
        timeToPreAnnounce = static_cast<uint32>(nowTime) + 1;
        diffToPreAnnounce = 1;
        preAnnounceSeconds = diffToShutdown;
    }

    LOG_INFO("module", "> ServerAutoShutdown ZoeCore: Next time to pre announce - {}", Acore::Time::TimeToHumanReadable(Seconds(timeToPreAnnounce)));
    LOG_INFO("module", "> ServerAutoShutdown ZoeCore: Remaining time to pre announce - {}", Acore::Time::ToTimeString<Seconds>(diffToPreAnnounce));
    LOG_INFO("module", " ");

    StartPersistentGameEvents();
    ScheduleExtraAnnouncements(diffToShutdown, preAnnounceSeconds);

    scheduler.Schedule(Seconds(diffToPreAnnounce), [preAnnounceSeconds](TaskContext /*context*/)
    {
        std::string preAnnounceMessageFormat = sConfigMgr->GetOption<std::string>("ServerAutoShutdown.PreAnnounce.Message", "|cffff2020[ZoeCore]|r Reinicio automatico do servidor em |cffffff00{}|r.");
        std::string message = BuildRestartMessage(preAnnounceMessageFormat, preAnnounceSeconds);

        BroadcastRestartMessage(message);
        sWorld->ShutdownServ(preAnnounceSeconds, SHUTDOWN_MASK_RESTART, SHUTDOWN_EXIT_CODE);
    });
}

void ServerAutoShutdown::OnUpdate(uint32 diff)
{
    if (!_isEnableModule)
        return;

    scheduler.Update(diff);
}

void ServerAutoShutdown::StartPersistentGameEvents()
{
    std::string eventList = sConfigMgr->GetOption<std::string>("ServerAutoShutdown.StartEvents", "");

    std::vector<std::string_view> tokens = Acore::Tokenize(eventList, ' ', false);
    GameEventMgr::GameEventDataMap const& events = sGameEventMgr->GetEventMap();

    for (auto token : tokens)
    {
        if (token.empty())
            continue;

        auto eventIdOpt = Acore::StringTo<uint32>(token);
        if (!eventIdOpt)
            continue;

        uint32 eventId = *eventIdOpt;
        sGameEventMgr->StartEvent(eventId);

        GameEventData const& eventData = events[eventId];
        LOG_INFO("module", "> ServerAutoShutdown ZoeCore: Starting event {} ({}).", eventData.Description, eventId);
    }
}
