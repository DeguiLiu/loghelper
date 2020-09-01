/* -8- ***********************************************************************
 *
 *  logger.hpp
 *
 *                                          Created by ogata on 11/26/2013
 *                 Copyright (c) 2013 ABEJA Inc. All rights reserved.
 ref:https://github.com/contaconta/boost_log_example/blob/master/logger.hpp
 * ******************************************************************** -8- */

#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <iostream>
#include <boost/filesystem.hpp>
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
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>

namespace logger
{
	namespace logging = boost::log;
	namespace sinks = boost::log::sinks;
	namespace attrs = boost::log::attributes;
	namespace src = boost::log::sources;
	namespace expr = boost::log::expressions;
	namespace keywords = boost::log::keywords;
	namespace prtree = boost::property_tree;

	// Complete sink type
	typedef sinks::synchronous_sink< sinks::syslog_backend > sink_t;

	static const char *kSysLogRoot = "SysLog";
	static const char *kSysLogAddr = "SysLogAddr";
	static const char *kSysLogPort = "SysLogPort";
	static const char *kFilelogMaxSize = "FilelogMaxSize";
	static const char *kFilelogMinFreeSpace = "FilelogMinFreeSpace";
    static const char *kSysLogLevel = "SysLogLevel";
    static const char *kFileLogLevel = "FileLogLevel";
    static const char *kConsoleLogLevel = "ConsoleLogLevel";

	static std::string s_SysLogTag = "MyLogger";

	struct LogConfig
	{
		std::string syslog_addr;
		int syslog_port;
		int filelogMaxSize = 1000;
		int filelogMinFreeSpace = 2000;
        int consolelog_evel = 1; // debug
        int filelog_evel = 4;   // error
        int syslog_evel = 2;    // info
	};

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

	// The formatting logic for the severity level
	template< typename CharT, typename TraitsT >
	inline std::basic_ostream< CharT, TraitsT >& operator<< (
		std::basic_ostream< CharT, TraitsT >& strm, severity_level lvl)
	{
		static const char* const str[] =
		{
			"TRACE",
			"DEBUG",
			"INFO",
			"WARNING",
			"ERROR",
			"FATAL",
			"Disable"
		};
		if (static_cast< std::size_t >(lvl) < (sizeof(str) / sizeof(*str)))
			strm << str[lvl];
		else
			strm << static_cast< int >(lvl);
		return strm;
	}

	static LogConfig loadSysLogConfig(const std::string &filepath)
	{
		prtree::ptree pt;
		prtree::ini_parser::read_ini(filepath, pt);
		prtree::ptree client;
		client = pt.get_child(kSysLogRoot);

		LogConfig cfg;
		auto a = client.get_optional<std::string>(kSysLogAddr);
		if (a)
			cfg.syslog_addr = *a;

		auto b = client.get_optional<int>(kSysLogPort);
		if (b)
			cfg.syslog_port = *b;

		auto c = client.get_optional<int>(kFilelogMaxSize);
		if (c)
			cfg.filelogMaxSize = *c;

		auto d = client.get_optional<int>(kFilelogMinFreeSpace);
		if (d)
			cfg.filelogMinFreeSpace = *d;

		auto e = client.get_optional<int>(kSysLogLevel);
		if (e)
			cfg.syslog_evel = *e;

        auto f = client.get_optional<int>(kFileLogLevel);
        if (f)
            cfg.filelog_evel = *f;

        auto g = client.get_optional<int>(kConsoleLogLevel);
        if (g)
            cfg.consolelog_evel = *g;
		return cfg;
	}
    
	static inline int initLogging(std::string& tag)
	{
		static bool inited = false;
		if (inited)
			return -1;

		const std::string logFilePath = boost::filesystem::current_path().string() + std::string("/logger.cfg");
		LogConfig cfg;
		if (boost::filesystem::exists(logFilePath))
			cfg = std::move(loadSysLogConfig(logFilePath));
		else
		{
			std::cerr << "logFilePath " << logFilePath << " not exist!" << std::endl;
			inited = true;
			return -1;
		}

		auto console_sink =logging::add_console_log(
			std::clog,
            keywords::filter = expr::attr< severity_level >("Severity") >= (severity_level)cfg.consolelog_evel, //设置打印级别
  			keywords::format = expr::stream
			<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
			<< " <" << expr::attr< severity_level >("Severity")
			<< "> " << expr::message);
	
		std::string fileName = "logs/";
		fileName.append(tag).append("-%Y-%m-%d_%N.log");
		auto file_sink = logging::add_file_log
		(
			keywords::filter = expr::attr< severity_level >("Severity") >= (severity_level)cfg.filelog_evel, //设置打印级别
			keywords::format = expr::stream
			<< expr::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S.%f")
			<< " <" << expr::attr< severity_level >("Severity")
			<< "> " << expr::message,
			keywords::file_name = fileName,		//文件名，注意是全路径
			keywords::rotation_size = 10 * 1024 * 1024,			//单个文件限制大小
			keywords::open_mode = std::ios_base::app			//追加
			//keywords::time_based_rotation = sinks::file::rotation_at_time_point(0, 0, 0)    //每天重建
		);

		file_sink->locked_backend()->set_file_collector(sinks::file::make_collector(
			keywords::target = "logs",							//文件夹名
			keywords::max_size = cfg.filelogMaxSize * 1024 * 1024,				//文件夹所占最大空间
			keywords::min_free_space = cfg.filelogMinFreeSpace * 1024 * 1024		//磁盘最小预留空间
		));

		// Create a new backend
		boost::shared_ptr< sinks::syslog_backend > backend(new sinks::syslog_backend(
            keywords::filter = expr::attr< severity_level >("Severity") >= (severity_level)cfg.syslog_evel,
			keywords::facility = sinks::syslog::local1,             /*< the logging facility >*/
			keywords::use_impl = sinks::syslog::udp_socket_based    /*< the built-in socket-based implementation should be used >*/
		));

		// Setup the target address and port to send syslog messages to
		backend->set_target_address(cfg.syslog_addr, cfg.syslog_port);

		// Create and fill in another level translator for "Severity" attribute of type string
		sinks::syslog::custom_severity_mapping< severity_level > mapping("Severity");
		mapping[TRACE] = sinks::syslog::notice;
		mapping[DEBUG] = sinks::syslog::debug;
		mapping[INFO] = sinks::syslog::info;
		mapping[WARNING] = sinks::syslog::warning;
		mapping[ERROR] = sinks::syslog::error;
		mapping[FATAL] = sinks::syslog::critical;
		backend->set_severity_mapper(mapping);

		file_sink->locked_backend()->scan_for_files();
		file_sink->locked_backend()->auto_flush(true);

		// Also let's add some commonly used attributes, like timestamp and record counter.
		logging::add_common_attributes();
		logging::core::get()->add_thread_attribute("Scope", attrs::named_scope());
		logging::core::get()->add_sink(console_sink);
		logging::core::get()->add_sink(file_sink);
		logging::core::get()->add_sink(boost::make_shared< sink_t >(backend));

        inited = true;

		return 0;
	}

} // end of namespace

BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(app_logger, boost::log::sources::severity_logger<logger::severity_level>)

#endif // LOGGER_HPP
