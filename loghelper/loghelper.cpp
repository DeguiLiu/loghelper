
#include "loghelper.h"
#include <stdint.h>
#include <sstream>
#include <iostream>
#include <thread>
#include <mutex>

using namespace RockLog;

#ifdef Use_QDebug		// For Qt Debuging
	#include <QDebug>	
#endif

#ifdef Use_SysLog
	#include "logger.hpp"		// boost.log, ref:https://github.com/contaconta/boost_log_example/blob/master/logger.hpp
#endif

LogHelper::LogHelper(int32_t level, const char* func, uint32_t line)
	:_level(level), _funcName(func), _lineNo(line)
{
}

int LogHelper::initLogHelper(std::string& tag)
{
	logger::s_SysLogTag = tag;
	static std::once_flag flag;
	int level;
	std::call_once(flag, [&] { level = logger::initLogging(logger::s_SysLogTag); });
	
	return level;
}

LogHelper::~LogHelper()
{
	std::ostringstream oss;

#if defined (Use_QDebug) ||  defined (Use_stdout)

	switch (_level)
	{
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
		break;
	}

#endif 

#ifdef Use_SysLog
	std::string tag = "\"" + logger::s_SysLogTag + "\"" + " ";
	oss << tag;
#endif

	oss << "[" << _funcName << ":" << _lineNo << "] - ";
	oss << _ss.str();

#ifdef Use_QDebug
	qDebug() << oss.str().c_str();
#endif

#ifdef Use_stdout
	std::cout << oss.str() << std::endl;
#endif

#ifdef Use_SysLog
    switch (_level)
	{
	case (int32_t)kDebug:
		BOOST_LOG_SEV(app_logger::get(), logger::DEBUG) << oss.str();
		break;
	case (int32_t)kInfo:
		BOOST_LOG_SEV(app_logger::get(), logger::INFO) << oss.str();
		break;
	case (int32_t)kWarn:
		BOOST_LOG_SEV(app_logger::get(), logger::WARNING) << oss.str();
		break;
	case (int32_t)kErr:
		BOOST_LOG_SEV(app_logger::get(), logger::ERROR) << oss.str();
		break;
	default:
		return;
	}
#endif
}
	




