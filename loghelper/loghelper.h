#pragma once

#include <stdint.h>
#include <sstream>
#include <iostream>

namespace RockLog {
	
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


enum LogLevel
{
	kDisable = -1,
	kDebug	 = 1,
	kInfo	 = 2,
	kWarn	 = 3,
	kErr	 = 4
};

class ROCK_LOG_API LogHelper
{
public:
	LogHelper(int32_t level, const char* func, uint32_t line);

	LogHelper&  operator<<(std::ostream& (*log)(std::ostream&))
	{
		_ss << log;
		return *this;
	}

	template <typename T>
	LogHelper& operator<<(const T& log) 
	{
		if (_ss.str().length() > 0)
		{
			_ss << " ";
		}
		_ss << log;
		return *this;
	}

	~LogHelper();
	static int initLogHelper(std::string& tag);

private:
	int32_t				_level;
	std::stringstream	_ss; 
	std::string			_funcName;
	uint32_t			_lineNo;
};

#define LOG(X)		LogHelper(X, __FUNCTION__, __LINE__)
}

