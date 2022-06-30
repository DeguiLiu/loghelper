// ref:https://github.com/contaconta/boost_log_example/blob/master/logger.hpp
// ref 多模块多文件参考：https://blog.csdn.net/jiafu1115/article/details/19936069

#ifndef LOGGER_HPP
#define LOGGER_HPP
#include "logconfigreader.hpp"
#include <iostream>
#include <boost/log/common.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>
#include <boost/log/attributes/timer.hpp>
#include <boost/log/attributes/named_scope.hpp>
#include <boost/log/sources/logger.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <mutex>

namespace logger
{
    namespace logging = boost::log;
    namespace sinks = boost::log::sinks;
    namespace attrs = boost::log::attributes;
    namespace src = boost::log::sources;
    namespace expr = boost::log::expressions;
    namespace keywords = boost::log::keywords;
    // Complete sink type
    typedef sinks::synchronous_sink<sinks::syslog_backend> sink_t;

    /**
     * @brief The severity_level enum
     *  Define application severity levels.
     */
    enum severity_level
    {
        TRACE = 0,
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL,
        Disable
    };

    std::mutex mtx;           // mutex for critical section
    ////// 线程局部变量 TLS
    thread_local std::string loggerTag;
    thread_local bool thread_inited = false;
    static std::map<std::string, src::severity_channel_logger<severity_level, std::string>> channel_map; // loggerTag-channel_logger

    // The formatting logic for the severity level
    template <typename CharT, typename TraitsT>
    inline std::basic_ostream<CharT, TraitsT> &operator<<(
        std::basic_ostream<CharT, TraitsT> &strm, severity_level lvl)
    {
        static const char *const str[] =
            {
                "TRACE",
                "DEBUG",
                "INFO",
                "WARNING",
                "ERROR",
                "FATAL",
                "Disable"};
        if (static_cast<std::size_t>(lvl) < (sizeof(str) / sizeof(*str)))
            strm << str[lvl];
        else
            strm << static_cast<int>(lvl);
        return strm;
    }

    static inline int initLogging(std::string tag)
    {
        std::lock_guard<std::mutex> lck (mtx);
        loggerTag = tag;
        if (channel_map.count(tag) > 0)
        {
            thread_inited = true;
            return 0;
        }
        if (!RockLog::LogConfigReader::instance().init())
            return -1;
        auto& cfg = RockLog::LogConfigReader::instance().cfg();
        auto console_sink = logging::add_console_log(
            std::clog,
            keywords::filter = (expr::attr<std::string>("Channel") == tag) && (expr::attr<severity_level>("Severity") >= (severity_level)cfg.consolelogLevel), //?????????
            keywords::format = expr::stream
                               << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                               << " <" << expr::attr<severity_level>("Severity")
                               << "> " << expr::message);

        std::string fileName = "logs/";
        if (!tag.empty())
            fileName.append(tag).append("_%Y-%m-%d_%N.log");
        else
            fileName.append("%Y-%m-%d_%N.log");
        auto file_sink = logging::add_file_log(

            keywords::filter = (expr::attr<std::string>("Channel") == tag) && (expr::attr<severity_level>("Severity") >= (severity_level)cfg.filelogLevel),
            keywords::format = expr::stream
                               << expr::format_date_time<boost::posix_time::ptime>("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
                               << " <" << expr::attr<severity_level>("Severity")
                               << "> " << expr::message,
            keywords::file_name = fileName,             //文件名，注意是全路径
            keywords::rotation_size = 10 * 1024 * 1024, //单个文件限制大小
            keywords::open_mode = std::ios_base::app    //追加
                                                        //keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0)    //每天重建
        );

        file_sink->locked_backend()->set_file_collector(sinks::file::make_collector(
            keywords::target = "logs",                                       //文件夹名
            keywords::max_size = cfg.filelogMaxSize * 1024 * 1024,           //文件夹所占最大空间
            keywords::min_free_space = cfg.filelogMinFreeSpace * 1024 * 1024 //磁盘最小预留空间
            ));

        // Create a new backend
        boost::shared_ptr<sinks::syslog_backend> backend(new sinks::syslog_backend(
            keywords::filter = (expr::attr<std::string>("Channel") == tag) && (expr::attr<severity_level>("Severity") >= (severity_level)cfg.syslogLevel),
            keywords::facility = sinks::syslog::local1,          /*< the logging facility >*/
            keywords::use_impl = sinks::syslog::udp_socket_based /*< the built-in socket-based implementation should be used >*/
            ));


        src::severity_channel_logger<severity_level, std::string> moduleOneLogger(keywords::channel = tag);
        // Also let's add some commonly used attributes, like timestamp and record counter.
        logging::add_common_attributes();
        logging::core::get()->add_thread_attribute("Scope", attrs::named_scope());
        logging::core::get()->add_sink(console_sink);
        logging::core::get()->add_sink(file_sink);

        if (!cfg.syslog_addr.empty() && cfg.syslog_port > 0)
        {
            // Setup the target address and port to send syslog messages to
            backend->set_target_address(cfg.syslog_addr, cfg.syslog_port);

            // Create and fill in another level translator for "Severity" attribute of type string
            sinks::syslog::custom_severity_mapping<severity_level> mapping("Severity");
            mapping[TRACE] = sinks::syslog::notice;
            mapping[DEBUG] = sinks::syslog::debug;
            mapping[INFO] = sinks::syslog::info;
            mapping[WARNING] = sinks::syslog::warning;
            mapping[ERROR] = sinks::syslog::error;
            mapping[FATAL] = sinks::syslog::critical;
            backend->set_severity_mapper(mapping);

            file_sink->locked_backend()->scan_for_files();
            file_sink->locked_backend()->auto_flush(true);

            logging::core::get()->add_sink(boost::make_shared<sink_t>(backend));
        }

        channel_map.insert(std::pair<std::string, src::severity_channel_logger<severity_level, std::string>>(tag, moduleOneLogger));
        thread_inited = true;
        return 0;
    }

} // namespace logger

#endif // LOGGER_HPP
