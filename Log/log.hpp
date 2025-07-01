#pragma once
#include <iostream>
#include <ctime>
#include <cstdarg>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <filesystem>

#define SCREEN_TYPE 1
#define FILE_TYPE 2

#define DeletFile()           \
    do                        \
    {                         \
        if (std::filesystem::exists(glogfile)) \
        {                     \
            std::filesystem::remove(glogfile); \
        }                     \
    } while (0)


const std::string glogfile = "./log.txt";

enum
{
    DEBUG = 1,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

std::string LevelToString(int level);

std::string GetCurrTime();

class logmessage
{
public:
    std::string _level;
    pid_t _id;
    std::string _filename;
    int _filenumber;
    std::string _curr_time;
    std::string _message_info;
};


class Log
{
public:
    Log(const std::string &logfile = glogfile) 
        : _logfile(logfile), _type(FILE_TYPE)
    {   
        DeletFile();
    }

    void Enable(int type)
    {
        _type = type;
    }

    void FlushLogToScreen(const logmessage &lg)
    {
        printf("[%s][%d][%s][%d][%s] %s",
               lg._level.c_str(),
               lg._id,
               lg._filename.c_str(),
               lg._filenumber,
               lg._curr_time.c_str(),
               lg._message_info.c_str());
    }

    void FlushLogToFile(const logmessage &lg)
    {
        std::ofstream out(_logfile, std::ios::out | std::ios::app);
        if (!out.is_open())
            return;
        char logtxt[2048];
        snprintf(logtxt, sizeof(logtxt), "[%s][%d][%s][%d][%s] %s%s",
                 lg._level.c_str(),
                 lg._id,
                 lg._filename.c_str(),
                 lg._filenumber,
                 lg._curr_time.c_str(),
                 lg._message_info.c_str(),
                 "\n");
        out.write(logtxt, strlen(logtxt));
        out.close();
    }

    void FlushLog(const logmessage &lg)
    {
        // 加过滤逻辑 --- TODO
        switch (_type)
        {
        case SCREEN_TYPE:
            FlushLogToScreen(lg);
            break;
        case FILE_TYPE:
            FlushLogToFile(lg);
            break;
        }
    }

    void logMessage(std::string filename, int filenumber, int level, const char *format, ...)
    {
        logmessage lg;

        lg._level = LevelToString(level);
        lg._id = getpid();
        lg._filename = filename;
        lg._filenumber = filenumber;
        lg._curr_time = GetCurrTime();

        va_list ap;
        va_start(ap, format);
        char log_info[1024];
        vsnprintf(log_info, sizeof(log_info), format, ap);
        va_end(ap);
        lg._message_info = log_info;

        // 打印出来日志
        FlushLog(lg);
    }

    ~Log()   {  }

private:
    int _type;
    std::string _logfile;
    std::ofstream out;
};

static Log lg;

#define LOG(Level, Format, ...)                                          \
    do                                                                   \
    {                                                                    \
        lg.logMessage(__FILE__, __LINE__, Level, Format, ##__VA_ARGS__); \
    } while (0)
#define EnableScreen()          \
    do                          \
    {                           \
        lg.Enable(SCREEN_TYPE); \
    } while (0)
#define EnableFILE()          \
    do                        \
    {                         \
        lg.Enable(FILE_TYPE); \
    } while (0)
