
#pragma once

#include "loghelper.h"
#include <boost/filesystem.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <atomic>

namespace prtree = boost::property_tree;

static const char *kSysLogRoot = "SysLog";
static const char *kSysLogAddr = "SysLogAddr";
static const char *kSysLogPort = "SysLogPort";
static const char *kFilelogMaxSize = "FilelogMaxSize";
static const char *kFilelogMinFreeSpace = "FilelogMinFreeSpace";
static const char *kSysLogLevel = "SysLogLevel";
static const char *kFileLogLevel = "FileLogLevel";
static const char *kConsoleLogLevel = "ConsoleLogLevel";
static const char *kUseBoostLog = "UseBoostLog";


namespace RockLog
{
class LogConfigReader
{
public:
    void loadSysLogConfig(const std::string &filepath)
    {
        prtree::ptree pt;
        prtree::ini_parser::read_ini(filepath, pt);
        prtree::ptree client;
        client = pt.get_child(kSysLogRoot);

        auto a = client.get_optional<std::string>(kSysLogAddr);
        if (a)
            _cfg.syslog_addr = *a;

        auto b = client.get_optional<int>(kSysLogPort);
        if (b)
            _cfg.syslog_port = *b;

        auto c = client.get_optional<int>(kFilelogMaxSize);
        if (c)
            _cfg.filelogMaxSize = *c;

        auto d = client.get_optional<int>(kFilelogMinFreeSpace);
        if (d)
            _cfg.filelogMinFreeSpace = *d;

        auto e = client.get_optional<int>(kSysLogLevel);
        if (e)
            _cfg.syslogLevel = *e;

        auto f = client.get_optional<int>(kFileLogLevel);
        if (f)
            _cfg.filelogLevel = *f;

        auto g = client.get_optional<int>(kConsoleLogLevel);
        if (g)
            _cfg.consolelogLevel = *g;

        auto h = client.get_optional<bool>(kUseBoostLog);
        if (h)
            _cfg.useBoostLog = *h;
    }

    bool init()
    {
        _isInit = true; // 只尝试加载配置文件一次
        try
        {
            std::string logFilePath = boost::filesystem::current_path().string() + std::string("/logger.cfg");
            if (boost::filesystem::exists(logFilePath))
            {
                loadSysLogConfig(logFilePath);
                return true;
            }
            else
            {
                std::cerr << "logFilePath " << logFilePath << " not exist!" << std::endl;
                return false;
            }
        }
        catch (const std::exception &e)
        {
            std::cout << e.what() << std::endl;
            return false;
        }
    }

    static LogConfigReader& instance()
    {
        static LogConfigReader ins;
        return ins;
    }

    LogConfig_t& cfg() { return _cfg; }
    bool isInit() { return _isInit; }
private:
    LogConfig_t _cfg;
    LogConfigReader(const LogConfigReader&) = delete;
    LogConfigReader& operator=(const LogConfigReader&) = delete;
    ~LogConfigReader() = default;

    LogConfigReader() = default;
    bool _isInit = false;
};
};

