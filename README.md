# vesper launcher: Vesper 启动器

与 [vesper](https://github.com/FlowerBlackG/vesper) 一同工作，用于在无人监管的服务器上自动启动 vesper，并在使用完毕后自动退出 linux 登录。

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
