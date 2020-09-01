#include "loghelper.h"
using namespace RockLog;

// ref: http://www.cplusplus.com/reference/thread/thread/?kw=thread
// thread example
#include <iostream>       // std::cout
#include <thread>         // std::thread

void foo()
{
    // do stuff...
    LOG(kDebug) << "[foo] debug ";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    LOG(kInfo) << "[foo] info... ";
    LOG(kErr) << "[foo] error!!! ";
}

void bar(int x)
{
    LOG(kDebug) << "[bar] debug ";
    LOG(kInfo) << "[bar] info... ";
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    LOG(kErr) << "[bar] error!!! ";
}

int main()
{
    std::string tag = "test_app";
    LogHelper::initLogHelper(tag);
    std::thread first(foo);     // spawn new thread that calls foo()
    std::thread second(bar, 0);  // spawn new thread that calls bar(0)

    std::cout << "main, foo and bar now execute concurrently...\n";

    // synchronize threads:
    first.join();                // pauses until first finishes
    second.join();               // pauses until second finishes

    std::cout << "foo and bar completed.\n";

    return 0;
}