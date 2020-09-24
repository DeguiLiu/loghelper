#pragma once

#include <stdint.h>
#include <sstream>
#include <iostream>

#if (!defined(ROCK_LIB) && !defined(ROCK_LINUX_PLATFORM))
    // ROCK is used as DLL
#define ROCK_DLLEXPORT __declspec(dllexport)
#define ROCK_DLLIMPORT __declspec(dllimport)
#else
#define ROCK_DLLEXPORT
#define ROCK_DLLIMPORT
#endif

#ifdef ROCK_LOG_EXPORTS
#define ROCK_LOG_API ROCK_DLLEXPORT
#else
#define ROCK_LOG_API ROCK_DLLIMPORT
#endif

namespace RockLog
{
    struct LogConfig_t
    {
        std::string syslog_addr;
        int syslog_port;
        int filelogMaxSize = 1000;
        int filelogMinFreeSpace = 2000;
        int consolelogLevel = 1; // debug
        int filelogLevel = 4;    // error
        int syslogLevel = 2;     // info
        bool useBoostLog = true; // 是否启用boost.log模块
    };

    enum LogLevel
    {
        kDisable = -1,
        kTrace = 0,
        kDebug = 1,
        kInfo = 2,
        kWarn = 3,
        kErr = 4
    };

    class ROCK_LOG_API LogHelper
    {
    public:
        LogHelper(int32_t level, const char *func, uint32_t line);
        LogHelper(int32_t level, const char *tag, const char *func, uint32_t line);
        LogHelper(int32_t level, std::string tag, const char *func, uint32_t line);

        LogHelper &operator<<(std::ostream &(*log)(std::ostream &))
        {
            // 如果不使用boost.log，并且控制台日志等级大于当前等级则立即返回
            if (!(s_cfg->useBoostLog == false && s_cfg->consolelogLevel> _level))
                _ss << log;
            return *this;
        }

        template <typename T>
        LogHelper &operator<<(const T &log)
        {
            if (!(s_cfg->useBoostLog == false && s_cfg->consolelogLevel> _level))
            {
                if (_ss.str().length() > 0)
                {
                    _ss << " ";
                }
                _ss << log;
            }
            return *this;
        }

        ~LogHelper();
        static int initLogHelper(std::string tag);
    protected:
        LogHelper() = default;
        LogHelper(const LogHelper&) = delete;
        LogHelper& operator=(const LogHelper&) = delete;

        static LogConfig_t* s_cfg;
        int32_t _level;
        std::stringstream _ss;
        std::string _funcName;
        uint32_t _lineNo;
        std::string _tag;
    };

    class ROCK_LOG_API Log2File
    {
    public:
        Log2File(std::string filename);
        ~Log2File();

        Log2File &operator<<(std::ostream &(*log)(std::ostream &))
        {
            _ss << log;
            return *this;
        }

        template <typename T>
        Log2File &operator<<(const T &log)
        {
            if (_ss.str().length() > 0)
            {
                _ss << " ";
            }
            _ss << log;
            return *this;
        }
        static void startConsumeThread();
    private:
        Log2File() = default;
        Log2File(const Log2File&) = delete;
        Log2File& operator=(const Log2File&) = delete;

        std::stringstream _ss;
        std::string _filename;
    };
} 

void ROCK_LOG_API rocklog(RockLog::LogLevel level, const char *func, uint32_t line, char *szFormat, ...);

#define LOG(X) LogHelper(X, __FUNCTION__, __LINE__)
#define LOG2(X, format, ...) rocklog(X, __FUNCTION__, __LINE__, format, ##__VA_ARGS__);
#define LOG_FUNC_LINE(X, FUNC, LINE) LogHelper(X, FUNC, LINE)
#define LOG_TAG(X, TAG) LogHelper(X, TAG, __FUNCTION__, __LINE__)
#define LOG2FILE(filename) Log2File(filename)
