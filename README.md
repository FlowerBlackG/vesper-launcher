# vesper launcher: Vesper 启动器

与 [vesper](https://github.com/FlowerBlackG/vesper) 一同工作，用于在无人监管的服务器上自动启动 vesper，并在使用完毕后自动退出 linux 登录。

## 姊妹项目

### vesper 核心

Wayland 图形合成器 & 窗口管理；VNC 远程桌面服务器。

[https://github.com/FlowerBlackG/vesper](https://github.com/FlowerBlackG/vesper)

### vesper 启动器

通过 domain socket 向刚登录的用户账户发送程序启动指令。

[https://github.com/FlowerBlackG/vesper-launcher](https://github.com/FlowerBlackG/vesper-launcher)

### vesper 中央服务

为前端操作台提供服务，控制 vesper 启动器与 vesper 核心工作。

[https://github.com/FlowerBlackG/vesper-center](https://github.com/FlowerBlackG/vesper-center)

### vesper 前端操作台

用户的操作界面。

[https://github.com/FlowerBlackG/vesper-front](https://github.com/FlowerBlackG/vesper-front)

## 功能

vesper-launcher 启动后，会开启一个 Unix Domain Socket。外层监管程序连接到 domain socket，向其中输入启动 vesper 的指令，然后断开与该 socket 的连接。

之后 launcher 会按照从 socket 收到的参数来启动 vesper，并在等待 vesper 运行结束后自动退出。

## 基本配置

建议在 linux 启动脚本里直接启动 vesper-launcher，并在其后直接添加一个退出命令。

例如，你可以这样配置 ~/.bash_profile

```bash
...
./vesper-launcher [命令行参数] > vesper-launcher.log
exit  # 执行完毕，立即退出登录。
```

## 命令行参数

### --version

显示 launcher 版本并退出。

当检测到此 flag 时，其他所有的命令行参数将被忽略。

### --usage / --help

显示使用帮助。

当此 flag 设置，且未设置 --version 时，将忽略其他所有命令行参数。

### --domain-socket [value]

指定 launcher 创建的 domain socket 名称。实际的 domain socket 文件将位于 `${XDG_RUNTIME_DIR}/[value]`

例：

```bash
./vesper-launcher --domain-socket v-launch.socket
```

## 必备的环境变量

* XDG_RUNTIME_DIR

## vesper-launcher socket 通信协议

本节中，协议格式表格的对齐单位都是 Byte。协议规定所有数字采用 Big Endian 格式存储。

表格中，从左往右、从上往下是字节流发送数据。即：第一行最左侧是最早发送的数据；第二行最左侧的信息紧随在第一行最右侧数据后发送。

**通用返回格式**是服务器向客户端发送应答时使用的。其他协议都是客户端向服务器发送数据时使用的。

### 协议头（header）

每个命令的开头都需要传输一个协议头。

```
    4B        4B
+---------+---------+
|  magic  |  type   |
+---------+---------+
|       length      |
+-------------------+
```

* magic (uint32): 协议魔数。固定为 `0x4663794F` （注：从低地址到高地址依次代表的ASCII字符为 `OycF`）
* type (uint32): 指令识别码。见具体指令
* length (uint64): 报文总长度（不含 header）

### 通用返回格式

`Response`

```
  8 Bytes
+-------------------+
|       header      |
+-------------------+
|       header      |
+---------+---------+
|  code   | msg len |
+---------+---------+
|      msg          |
|      ...          |

```

* type (uint32): `0xA001`
* code (uint32): 状态码。0 表示正常
* msg len (uint32): 返回信息长度。单位为 Byte
* msg (byte array): 返回信息。不包含尾 0

### 执行 /bin/sh 启动命令

`ShellLaunch`

```
     8 Bytes
+----------------+
|     header     |
+----------------+
|     header     |
+----------------+
|   cmd length   |
+----------------+
|      cmd       |
|      ...       |
```

* type (uint32): `0x0001`
* cmd length (uint64): cmd 字段的长度。单位为 Byte
* cmd (byte array): 参数。字节长度由 cmd length 指定。命令结尾不要添加尾 0

该指令正确执行时，launcher 会 fork 一个子进程，并使用 `/bin/sh` 执行如下指令：

```bash
/bin/sh -c cmd
```

其中，`cmd` 会被当成一整个参数发送到命令行。

launcher 的父进程会等待子进程，在后者执行完毕后直接退出。

该指令执行成功时，会令 launcher server 向 client 发送应答信息后立即断开连接。
