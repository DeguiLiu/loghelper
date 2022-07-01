#pragma once

#include <stdint.h>
#include <sstream>
#include <iostream>
#include <vector>
#include <regex>
#include <type_traits>
#include <sstream>
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
#include <string.h>
#ifdef __linux__
#define __FILENAME__ (strrchr(__FILE__, '/') ? strrchr(__FILE__, '/') + 1 : __FILE__)
#else
#define __FILENAME__ (strrchr(__FILE__, '\\') ? strrchr(__FILE__, '\\') + 1 : __FILE__)
#endif
    struct LogConfig_t
    {
        std::string syslog_addr;
        int syslog_port;
        int filelogMaxSize = 1000;
        int filelogMinFreeSpace = 2000;
        int consolelogLevel = 2; // debug
        int filelogLevel = 2;    // error
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
        LogHelper(int32_t level, const char *file, const char *func, uint32_t line);
        LogHelper(int32_t level, const char *tag, const char *file, const char *func, uint32_t line);
        LogHelper(int32_t level, std::string tag, const char *file, const char *func, uint32_t line);

        LogHelper &operator<<(std::ostream &(*log)(std::ostream &))
        {
            // 如果不使用boost.log，并且控制台日志等级大于当前等级则立即返回
            if (!(s_cfg->useBoostLog == false && s_cfg->consolelogLevel > _level))
                _ss << log;
            return *this;
        }

        template <typename T>
        LogHelper &operator<<(const T &log)
        {
            if (!(s_cfg->useBoostLog == false && s_cfg->consolelogLevel > _level))
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
        LogHelper(const LogHelper &) = delete;
        LogHelper &operator=(const LogHelper &) = delete;

        static LogConfig_t *s_cfg;
        int32_t _level;
        std::stringstream _ss;
        std::string _funcName;
        std::string _fileName;
        uint32_t _lineNo;
        std::string _tag;
    };
}

void ROCK_LOG_API rocklog(RockLog::LogLevel level, const char *file, const char *func, uint32_t line, char *szFormat, ...);

#define LOG(X) RockLog::LogHelper(X, __FILENAME__, __FUNCTION__, __LINE__)
#define LOG2(X, format, ...) rocklog(X, __FILENAME__, __FUNCTION__, __LINE__, (char *)format, ##__VA_ARGS__)
#define LOG_TAG(X, TAG) RockLog::LogHelper(X, TAG, __FILENAME__, __FUNCTION__, __LINE__)
#define LOG_FUNC_LINE(X, FILENAME, FUNC, LINE) RockLog::LogHelper(X, FILENAME, FUNC, LINE)

///////////////// for AMS ////////////////////
//ref:https://blog.csdn.net/10km/article/details/80216226
template <typename E,
          typename TR = std::char_traits<E>,
          typename T>
typename std::enable_if<!std::is_pointer<T>::value>::type
_value_output_stream(std::basic_ostream<E, TR> &stream, const T &value)
{
    stream << value;
}

template <typename E,
          typename TR = std::char_traits<E>,
          typename T>
typename std::enable_if<std::is_pointer<T>::value>::type
_value_output_stream(std::basic_ostream<E, TR> &stream, const T &value)
{
    if (nullptr == value)
    {
        stream << "null";
    }
    else
    {
        stream << value;
    }
}

template <typename E,
          typename TR = std::char_traits<E>,
          typename AL = std::allocator<E>>
inline bool end_with(const std::basic_string<E, TR, AL> &src, const std::basic_string<E, TR, AL> &suffix)
{
    if (src.size() < suffix.size())
    {
        return false;
    }
    return src.substr(src.size() - suffix.size()) == suffix;
}

template <typename E,
          typename TR = std::char_traits<E>,
          typename AL = std::allocator<E>>
inline bool end_with(const std::basic_string<E, TR, AL> &src, const E *suffix)
{
    return end_with(src, std::basic_string<E, TR, AL>(suffix));
}

template <typename E,
          typename TR = std::char_traits<E>,
          typename AL = std::allocator<E>,
          typename _str_type = std::basic_string<E, TR, AL>>
std::vector<_str_type> split(const std::basic_string<E, TR, AL> &in, const E *delim)
{
    return split(in, _str_type(delim));
}
// c string版本
inline std::vector<std::string> split(const char *in, const char *delim)
{
    std::regex re{delim};
    return std::vector<std::string>{
        std::cregex_token_iterator(in, in + strlen(in), re, -1),
        std::cregex_token_iterator()};
}

template <typename E,
          typename TR = std::char_traits<E>,
          typename AL = std::allocator<E>,
          typename _str_type = std::basic_string<E, TR, AL>>
std::vector<_str_type> split(const std::basic_string<E, TR, AL> &in, const std::basic_string<E, TR, AL> &delim)
{
    std::basic_regex<E> re{delim};
    return std::vector<_str_type>{
        std::regex_token_iterator<typename _str_type::const_iterator>(in.begin(), in.end(), re, -1),
        std::regex_token_iterator<typename _str_type::const_iterator>()};
}

template <typename E,
          typename TR = std::char_traits<E>,
          typename AL = std::allocator<E>>
void _sm_log_output(std::basic_ostream<E, TR> &stream, const std::vector<std::basic_string<E, TR, AL>> &format, int &idx)
{
}

template <typename E,
          typename TR = std::char_traits<E>,
          typename AL = std::allocator<E>,
          typename T, typename... Args>
void _sm_log_output(std::basic_ostream<E, TR> &stream, const std::vector<std::basic_string<E, TR, AL>> &format, int &idx, const T &first, Args... rest)
{
    if (idx < (int)format.size())
    {
        _value_output_stream(stream, format[idx]);
        if (idx < (int)format.size() - 1)
        {
            _value_output_stream(stream, first);
        }
        _sm_log_output(stream, format, ++idx, rest...);
    }
}

template <typename E,
          typename TR = std::char_traits<E>,
          typename AL = std::allocator<E>,
          typename _str_type = std::basic_string<E, TR, AL>,
          typename... Args>
void sample_log(RockLog::LogLevel level, const char *file, const char *func, int line, const std::basic_string<E, TR, AL> &format, Args... args)
{
    const static char delim[] = "{}";

    // {}为占位符
    auto vf = split(format, std::string("\\{\\}"));
    if (end_with(format, delim))
    {
        vf.push_back(_str_type());
    }
    std::stringstream stream;
    int index = 0;
    _sm_log_output(stream, vf, index, args...);
    for (; index < vf.size(); ++index)
    {
        stream << vf[index];
        if (index < (int)vf.size() - 1)
        {
            stream << delim;
        }
    }
    LOG_FUNC_LINE(level, file, func, line) << stream.str();
}
// 局部特化函数
// 当format为指针类型时，转为string
template <typename E,
          typename TR = std::char_traits<E>,
          typename AL = std::allocator<E>,
          typename... Args>
void sample_log(RockLog::LogLevel level, const char *file, const char *func, int line, const E *format, Args... args)
{
    sample_log(level, file, func, line, std::basic_string<E, TR, AL>(format), args...);
}

#define AMS_DEBUG(format, ...) sample_log(kDebug, __FILENAME__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define AMS_INFO(format, ...) sample_log(kInfo, __FILENAME__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define AMS_WARN(format, ...) sample_log(kWarn, __FILENAME__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
#define AMS_ERR(format, ...) sample_log(kErr, __FILENAME__, __FUNCTION__, __LINE__, format, ##__VA_ARGS__)
///////////////// for AMS ////////////////////
