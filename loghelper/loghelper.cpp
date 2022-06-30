
#include "loghelper.h"
#include <stdint.h>
#include <sstream>
#include <iostream>
#include "logger.hpp" // boost.log, ref:https://github.com/contaconta/boost_log_example/blob/master/logger.hpp
#include <stdarg.h>
#include "logconfigreader.hpp"

using namespace RockLog;


LogConfig_t *LogHelper::s_cfg = &LogConfigReader::instance().cfg();
LogHelper::LogHelper(int32_t level, const char *file, const char *func, uint32_t line)
    : _level(level), _funcName(func), _lineNo(line), _fileName(file)
{
    if (!RockLog::LogConfigReader::instance().isInit())
        RockLog::LogConfigReader::instance().init();
}

LogHelper::LogHelper(int32_t level, const char *tag, const char *file, const char *func, uint32_t line)
    : _level(level), _tag(tag), _funcName(func), _lineNo(line), _fileName(file)
{
    if (!RockLog::LogConfigReader::instance().isInit())
        RockLog::LogConfigReader::instance().init();
}

LogHelper::LogHelper(int32_t level, std::string tag, const char *file, const char *func, uint32_t line)
    : _level(level), _tag(tag), _funcName(func), _lineNo(line), _fileName(file)
{
    if (!RockLog::LogConfigReader::instance().isInit())
        RockLog::LogConfigReader::instance().init();
}

int LogHelper::initLogHelper(std::string tag)
{
    if (s_cfg->useBoostLog)
    {
        int ret = logger::initLogging(tag);
        if (0 != ret)
        {
            std::cerr << "logger::initLogging failed!" << std::endl;
            s_cfg->useBoostLog = false;   // logger::initLogging不成功则使用默认的简单的日志输出
        }
        return ret;
    }
    else
    {
        return 0;
    }
}

LogHelper::~LogHelper()
{
    // 如果不使用boost.log，并且控制台日志等级大于当前等级则立即返回
    if (!s_cfg->useBoostLog && s_cfg->consolelogLevel > _level)
        return;

    std::ostringstream oss;

    if (!s_cfg->useBoostLog)
    {
        switch (_level)
        {
        case (int32_t)kTrace:
            oss << "[T] -";
            break;
        case (int32_t)kDebug:
            oss << "[D] -";
            break;
        case (int32_t)kInfo:
            oss << "[I] -";
            break;
        case (int32_t)kWarn:
            oss << "[W] -";
            break;
        case (int32_t)kErr:
            oss << "[E] -";
            break;
        case (int32_t)kDisable:
            return;
        default:
            return;
        }

        oss << "[" << _fileName << " " << _funcName << ":" << _lineNo << "] - "<< _ss.str();

        std::cout << oss.str() << std::endl << std::flush;
        oss.clear();
    }
    else
    {
        try
        {
            std::string tag = _tag;
            if (!tag.empty())
            {
                tag = "{" + tag + "}";
                oss << tag;
            }
            oss << " [" << _fileName << " " << _funcName << ":" << _lineNo << "] - " << _ss.str();

            boost::log::sources::severity_channel_logger<logger::severity_level, std::string> logger;
            if (logger::channel_map.count(_tag) == 0)
                logger::initLogging(_tag);

            logger = logger::channel_map[_tag];
            switch (_level)
            {
            case (int32_t)kTrace:
                BOOST_LOG_SEV(logger, logger::TRACE) << oss.str();
                break;
            case (int32_t)kDebug:
                BOOST_LOG_SEV(logger, logger::DEBUG) << oss.str();
                break;
            case (int32_t)kInfo:
                BOOST_LOG_SEV(logger, logger::INFO) << oss.str();
                break;
            case (int32_t)kWarn:
                BOOST_LOG_SEV(logger, logger::WARNING) << oss.str();
                break;
            case (int32_t)kErr:
                BOOST_LOG_SEV(logger, logger::ERROR) << oss.str();
                break;
            default:
                return;
            }
        }
        catch(const std::exception& e)
        {
            std::cerr << e.what() << '\n';
            return;
        }

    }
    _ss.clear();
}

void rocklog(RockLog::LogLevel level, const char *file, const char *func, uint32_t line, char *szFormat, ...)
{
    const uint32_t MAX_LOG_MSG_LEN = 6000; // 每条日志的最大长度
    va_list pvList;
    char msg[MAX_LOG_MSG_LEN] = {0};
    uint32_t len = 0;

    if (szFormat == NULL)
        return;
    va_start(pvList, szFormat);
    len = vsprintf(msg, szFormat, pvList);
    va_end(pvList);
    if (len <= 0 || len >= MAX_LOG_MSG_LEN)
        return;
    LOG_FUNC_LINE(level, file, func, line) << msg;
}
