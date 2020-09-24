+ 需提前安装boost开发包；
+ 基于boost.log的日志模块，供C++程序使用；
+ 动态链接库方式提供模块，非嵌入式；

### 写日志

- 支持多种不同的输出目的地，满足：
  - 输出到本地文件，支持按日期存储，支持按大小回滚；
  - 支持rsyslog协议，以将日志发往远程日志服务器

+ 支持类似std::cout的打印方式：`LOG(kErr) << "error";` ，也支持printf打印方式:`LOG2(kErr, "error %d", 100);` 。
    ```
     LOG(kErr) << "[foo] error!!! ";
     LOG2(kErr, "%s, %d", "test log", 100);
    ```
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
; 是否使用boost.log;如果不使用，则启用std::cout输出至屏幕上，这将有效减少日志本身消耗
UseBoostLog = true

```

说明：如果设置`UseBoostLog = false`，则关闭boost.log的使用，转而启用std::cout输出至屏幕上

+ 输出结果示例

调用代码如下：

```
2020-09-24 17:38:39.118359 <DEBUG> "FOO" [foo:16] - [foo] debug
2020-09-24 17:38:39.120355 <DEBUG> "BAR" [bar:31] - [bar] debug
2020-09-24 17:38:39.138305 <INFO> "FOO" [foo:18] - [foo] info...
2020-09-24 17:38:39.139303 <ERROR> "FOO" [foo:19] - [foo] error!!!
2020-09-24 17:38:39.142295 <ERROR> "FOO" [foo:20] - test log, 100
2020-09-24 17:38:39.155260 <INFO> "BAR" [bar:33] - [bar] info...
2020-09-24 17:38:39.155260 <ERROR> "BAR" [foo:22] - [foo] error!!!
2020-09-24 17:38:39.170223 <ERROR> "BAR" [bar:35] - [bar] error!!
```

### 写文件

+ 使用方式

  ```
      std::string filename = std::string("abc/baz.txt");
      std::stringstream ss;
      ss << "a" << "|" << "b" << std::endl;
      LOG2FILE(filename) << ss.str();  						// 1)
      LOG2FILE(filename) << "a" << "|" << "b" << std::endl;	// 2)
  ```

如调用`Log2File::startConsumeThread();`，则开启生产者消费者机制，先将文件内容写到内存中，再由单独线程写入到文件；

减少因写文件而增加的时延，多线程写文件情况下性能也更高。

