#include "log.hpp"

std::string LevelToString(int level)
{
    switch (level)
    {
    case DEBUG:
        return "DEBUG";
    case INFO:
        return "INFO";
    case WARNING:
        return "WARNING";
    case ERROR:
        return "ERROR";
    case FATAL:
        return "FATAL";
    default:
        return "UNKNOWN";
    }
}

std::string GetCurrTime()
{
    time_t now = time(nullptr);
    struct tm *curr_time = localtime(&now);
    char buffer[128];
    snprintf(buffer, sizeof(buffer), "%d-%02d-%02d %02d:%02d:%02d",
             curr_time->tm_year + 1900,
             curr_time->tm_mon + 1,
             curr_time->tm_mday,
             curr_time->tm_hour,
             curr_time->tm_min,
             curr_time->tm_sec);
    return buffer;
}

