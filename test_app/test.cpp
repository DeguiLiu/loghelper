#include "loghelper.h"
using namespace RockLog;

// ref: http://www.cplusplus.com/reference/thread/thread/?kw=thread
// thread example
#include <iostream>       // std::cout
#include <thread>         // std::thread
#include <boost/filesystem.hpp>

void foo()
{
    std::string tag = "FOO";
    LogHelper::initLogHelper(tag);
    // do stuff...
    LOG(kTrace) << "[foo] trace ";
    LOG(kDebug) << "[foo] debug ";
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    LOG(kInfo) << "[foo] info... ";
    LOG(kErr) << "[foo] error!!! ";
    LOG2(kErr, "%s, %d", "test log", 100);
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    LOG_TAG(kErr, "BAR") << "[foo] error!!! ";
}

void bar(int x)
{
    std::string tag = "BAR";
    LogHelper::initLogHelper(tag);

    LOG(kTrace) << "[bar] trace ";
    LOG(kDebug) << "[bar] debug ";
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    LOG(kInfo) << "[bar] info... ";
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    LOG(kErr) << "[bar] error!!! ";
}

void baz()
{
    std::string filename = std::string("abc/baz.txt");
    std::stringstream ss;
    ss << "a" << "|" << "b" << std::endl;
    LOG2FILE(filename) << ss.str();
    LOG2FILE(filename) << "a" << "|" << "b" << std::endl;
}

int main()
{
    std::string tag = "MAIN";
    LogHelper::initLogHelper(tag);
    Log2File::startConsumeThread();
    std::thread first(foo);     // spawn new thread that calls foo()
    std::thread second(bar, 0);  // spawn new thread that calls bar(0)
    std::thread third(baz);  // spawn new thread that calls baz()

    std::cout << "execute concurrently...\n";

    // synchronize threads:
    first.join();
    second.join();
    third.join();

    std::cout << " completed.\n";
    return 0;
}
