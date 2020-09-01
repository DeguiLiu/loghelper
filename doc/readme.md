**C++日志模块主要特点如下**：

+ 动态链接库方式提供模块，非嵌入式；

- 支持多种不同的输出目的地，满足：
  - 输出到本地文件，支持按日期存储，支持按大小回滚；
  - 支持rsyslog协议，以将日志发往远程日志服务器
  - QDebug输出，方便Qt程序调用；
- 尽量保留C++标准输出的习惯，如Log(kDebug) << "aaa" << 1<<std::endl;

**日志模块的设计**：

![日志模块设计](日志模块设计.png)

- 主要是利用了C++构造和自动析构的机制，并8.重载<<操作符来保留使用习惯；
- 在析构构造函数中，组织消息拼接；
- 根据使用的宏将拼接好的消息交给不同日志输出模块
- 借鉴github的Boost.Log封装，[boost_log_example](https://github.com/contaconta/boost_log_example) 和 [simpleLogger]( https://github.com/gklingler/simpleLogger）

