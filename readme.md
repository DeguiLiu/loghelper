+ 支持windows和linux平台，需自己安装boost开发包；
+ 基于boost.log的日志模块，供C++程序使用；
+ 动态链接库方式提供模块，非嵌入式；

- 支持多种不同的输出目的地，满足：
  - 输出到本地文件，支持按日期存储，支持按大小回滚；
  - 支持rsyslog协议，以将日志发往远程日志服务器
  - QDebug输出，方便Qt程序调用；

+ 使用示例见test_app程序。
+ 配置文件示例

```ini
[SysLog]

; syslog日志服务器地址
SysLogAddr = 192.168.10.199
; syslog日志服务器端口     
SysLogPort = 514  

; 文件日志目录最大占用磁盘空间，单位：M
FilelogMaxSize = 100

; -1:不产生日志，1：debug以上级别，
; 2：info以上级别，3：warning以上级别，4：error以上级别
; console日志等级
ConsoleLogLevel = 1
; 写入文件日志等级
FileLogLevel = 4
; syslog日志等级
SysLogLevel = 2
```



+ 输出结果示例

```
2020-08-31 18:02:20.103086 <DEBUG> "test_app" [foo:11] - [foo] debug
2020-08-31 18:02:20.103086 <DEBUG> "test_app" [bar:19] - [bar] debug
2020-08-31 18:02:20.106078 <INFO> "test_app" [bar:20] - [bar] info...
2020-08-31 18:02:20.212792 <INFO> "test_app" [foo:13] - [foo] info...
2020-08-31 18:02:20.213794 <ERROR> "test_app" [foo:14] - [foo] error!!!
2020-08-31 18:02:20.212792 <ERROR> "test_app" [bar:22] - [bar] error!!!
```

