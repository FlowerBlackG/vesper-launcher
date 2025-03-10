// SPDX-License-Identifier: MulanPSL-2.0

/*
 * Vesper Launcher Entry
 * 创建于 2024年2月25日 京沪高铁上
 */

#include <iostream>
#include <set>
#include <map>
#include <string>
#include <vector>
#include <memory>
#include <sstream>
#include <unistd.h>

#include "./Log.h"
#include "./config.h"
#include "./Protocols.h"

#include <fcntl.h>
#include <signal.h>

#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>
#include <sys/stat.h>

#include <filesystem>

using namespace std;


/* ------------ 全局变量 ------------ */

static struct {
    map<string, string> variables;
    set<string> flags;
    vector<string> values;
} userArgs;

static map<string, string> envVars;

static struct {
    struct {
        string xdgRuntimeDir;
    } environment;

    string domainSocket;

    bool daemonize;
    bool serviceMode;
    bool waitForChildBeforeExit;

    bool quitIfVesperCtrlLive;
    string vesperCtrlSockAddr;  // 相对 $XDG_RUNTIME_DIR
} config;


static bool systemRunning;
static int socketListenFd = -1;

/* ------------ "一句话"指令 ------------ */

static void usage() {
    cout << "vesper-launcher " << VESPER_LAUNCHER_VERSION_NAME << endl;
    cout << "check ";
    ConsoleColorPad::setCoutColor(0x2f, 0x90, 0xb9);
    cout << "https://github.com/FlowerBlackG/vesper-launcher";
    ConsoleColorPad::setCoutColor();
    cout << " for usage." << endl;
}

static void version() {
    cout << "vesper launcher" << endl;
    cout << "  version: "
        << VESPER_LAUNCHER_VERSION_NAME
        << " (" 
        << VESPER_LAUNCHER_VERSION_CODE  
        << ")"
        << endl
        << "  built on "
        << VESPER_LAUNCHER_BUILD_TIME_HUMAN_READABLE << endl;
}


/* ------------ 命令行解析 ------------ */


static struct {
    bool initialized = false;
    set<string> flagKeys;
    set<string> valueKeys;
} predefinedArgKeys;

static void loadPredefinedArgKeys() {

    struct {
        const char* key;
        bool isFlag;
    } keys[] = {
        { "--version", true },
        { "--usage", true },
        { "--help", true },
        { "--domain-socket", false },
        { "--daemonize", true },
        { "--service-mode", true },
        { "--wait-for-child-before-exit", true },
        { "--no-color", true },
        { "--quit-if-vesper-ctrl-live", true },
        { "--vesper-ctrl-sock-addr", false }
    };

    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
        auto& it = keys[i];
        (it.isFlag ? &predefinedArgKeys.flagKeys : &predefinedArgKeys.valueKeys)->insert(it.key);
    }
    predefinedArgKeys.initialized = true;
}

static int parseArgs(int argc, const char** argv) {

    if (!predefinedArgKeys.initialized) {
        loadPredefinedArgKeys();
    }

    const auto& isKey = [] (string& s) {
        return s.starts_with("--");
    };


    for (int idx = 1; idx < argc; idx++) {
        string key = argv[idx];
        if (predefinedArgKeys.flagKeys.contains(key)) {
            if (userArgs.flags.contains(key)) {
                LOG_WARN("flag redefined: ", key);
            } else {
                userArgs.flags.insert(key);
            }

            continue;
        } else if (predefinedArgKeys.valueKeys.contains(key)) {
            if (idx + 1 == argc) {
                LOG_ERROR("no value for key ", key);
                return -1;
            }

            string value = argv[++idx];
            if (isKey(value)) {
                LOG_ERROR("key ", key, " followed by another key ", value);
                return -1;
            }

            if (userArgs.variables.contains(key)) {
                LOG_WARN("key ", key, " redefined. ");
                LOG_WARN("  value |", userArgs.variables[key], "| is replaced by |", value, "|.");
            }

            userArgs.variables[key] = value;
        } else if (isKey(key)) {
            LOG_ERROR("key ", key, " is not defined.")
            return -1;
        } else {
            userArgs.values.push_back(key);
        }
    }


    return 0;
}

static void processEnvVars(const char** env) {
    for (int i = 0; env[i]; i++) {
        string it = env[i];
        auto splitPos = it.find('=');
        if (splitPos == string::npos) {
            LOG_WARN("no '=' in env variable: ", it);
            continue;
        }
        envVars[it.substr(0, splitPos)] = it.substr(splitPos + 1);
    }
}

static int processPureQueryCmds() {
    if (userArgs.flags.contains("--version")) {
        version();
        return 1;
    } 
    
    if (userArgs.flags.contains("--usage") || userArgs.flags.contains("--help")) {
        usage();
        return 1;
    }

    return 0;
}


/* ------------ 守护进程 ------------ */

static int daemonize() {
    pid_t pid = fork();
    if (pid < 0) {
        cout << "error: failed to daemonize!" << endl;
        return -1;
    }

    // 父进程结束运行。
    if (pid != 0) {
        exit(0);
    }

    // 创建新会话。
    if ((pid = setsid()) < -1) {
        cout << "error: failed to set sid!" << endl;
        return -1;
    }

    // 更改当前工作目录。
    chdir("~");

    signal(SIGTERM, [] (int arg) {
        systemRunning = false;
        if (socketListenFd != -1) {
            shutdown(socketListenFd, SHUT_RDWR);
        }
    });

    return 0;
}


static int buildConfig() {
    if (!envVars.contains("XDG_RUNTIME_DIR")) {
        cout << "error: XDG_RUNTIME_DIR required but not set." << endl;
        usage();
        return -1;
    } else {
        config.environment.xdgRuntimeDir = envVars["XDG_RUNTIME_DIR"];
    }

    if (!userArgs.variables.contains("--domain-socket")) {
        cout << "error: --domain-socket required but not found." << endl;
        usage();
        return -2;
    } else {
        config.domainSocket = userArgs.variables["--domain-socket"];
    }

    config.daemonize = userArgs.flags.contains("--daemonize");
    config.serviceMode = userArgs.flags.contains("--service-mode");
    config.waitForChildBeforeExit = userArgs.flags.contains("--wait-for-child-before-exit");
    config.quitIfVesperCtrlLive = userArgs.flags.contains("--quit-if-vesper-ctrl-live");

    if (config.quitIfVesperCtrlLive) {
        const string vesperCtrlSockAddrCmdlineKey = "--vesper-ctrl-sock-addr";
        if (!userArgs.variables.contains(vesperCtrlSockAddrCmdlineKey)) {
            cout << "--quit-if-vesper-ctrl-live should be paired with " 
                << vesperCtrlSockAddrCmdlineKey << endl;
            return -4;
        }

        config.vesperCtrlSockAddr = userArgs.variables[vesperCtrlSockAddrCmdlineKey];
    }

    if (config.daemonize && config.serviceMode) {
        cout << "--daemonize and --service-mode should not come together. confusing!" << endl;
        return -3;
    }
    
    return 0;
}


/* ------------ 网络 ------------ */

static void sendResponse(int connFd, uint32_t code, const string& msg) {
    vl::protocol::Response response;
    response.code = code;
    response.msg = msg;
    
    stringstream s(ios::in | ios::out | ios::binary);
    response.encode(s);

    string str = s.str();
    write(connFd, str.c_str(), str.length());
}


/**
 * 
 * 
 * 
 * @return 异常退出时，返回负数；
 *         正常退出时，如果希望结束监听，不再服务下一个 client，返回0；否则返回正数。
 */
static int processProtocol(vl::protocol::Base* protocol, int connFd) {
    auto protocolType = protocol->getType();
    
    if (protocolType == vl::protocol::ShellLaunch::typeCode) {
        auto* p = (vl::protocol::ShellLaunch*) protocol;
        pid_t pid = fork();
        if (pid < 0) {
            const char* errMsg = "failed to create subprocess!";
            LOG_ERROR(errMsg);
            sendResponse(connFd, 1, errMsg);
            return 1;
        } else if (pid == 0) { // new process
            execl("/bin/sh", "/bin/sh", "-c", p->cmd.c_str(), nullptr);
            exit(-1);
        }

        sendResponse(connFd, 0, "");

        return 0;
    } else {
        LOG_ERROR("type unrecognized: ", protocol->getType());
        return 1;
    }
}

static int readNBytesFromSocket(int connFd, int n, char* buf) {
    int totalBytesRead = 0;
    char* dataPtr = buf;
    while (totalBytesRead < n) {
        int bytes = read(connFd, dataPtr, n - totalBytesRead);
        if (bytes < 0) {
            LOG_ERROR("read error! nBytes is ", n);
            return -2;
        } else if (bytes == 0) {
            LOG_ERROR("unexpected EOF from socket.");
            return -3;
        }

        totalBytesRead += bytes;
        dataPtr += bytes;
    }

    return 0;
}


/**
 * 
 * 
 * 
 * @return 异常退出时，返回负数；
 *         正常退出时，如果希望结束监听，返回0；否则返回正数。
 */
static int acceptClient(int listenFd, vector<char>& bufContainer) {

    sockaddr_un client;
    socklen_t clientLen = sizeof(client);

    int connFd = accept(listenFd, (sockaddr*) &client, &clientLen);
    if (connFd < 0) {

        if (!systemRunning) {
            return 0;
        }

        LOG_WARN("socket connection failed.");
        return 1;
    }

    int resCode = 0;
    do {

        auto dataPtr = bufContainer.data();

        // 读入 header

        if (readNBytesFromSocket(connFd, vl::protocol::HEADER_LEN, dataPtr)) {
            const char* err = "failed to read header!";
            LOG_ERROR(err);
            sendResponse(connFd, 2, err);
            resCode = 2;
            break;
        }

        if (strncmp(dataPtr, vl::protocol::MAGIC_STR, 4)) {
            const char* err = "magic mismatched!";
            LOG_ERROR(err);
            sendResponse(connFd, 3, err);
            resCode = 3;
            break;
        }
        dataPtr += 4;

        uint32_t type = be32toh(*(uint32_t*) dataPtr);
        dataPtr += 4;

        uint64_t length = be64toh(*(uint64_t*) dataPtr);
        dataPtr += 8;

        if (length + vl::protocol::HEADER_LEN > bufContainer.capacity())
            bufContainer.reserve(length + vl::protocol::HEADER_LEN);

        dataPtr = bufContainer.data();

        if (readNBytesFromSocket(connFd, length, dataPtr + vl::protocol::HEADER_LEN)) {
            const char* err = "failed to read body!";
            LOG_ERROR(err);
            sendResponse(connFd, 5, err);
            resCode = 5;
            break;
        }
    
        vl::protocol::Base* protocol = vl::protocol::decode(dataPtr, type, length + vl::protocol::HEADER_LEN);

        if (protocol == nullptr) {
            const char* err = "failed to parse protocol!";
            LOG_ERROR(err);
            sendResponse(connFd, 7, err);
            resCode = 6;
            break;
        }

        resCode = processProtocol(protocol, connFd);

    } while (0); // end of do-while(0)

    close(connFd);
    return resCode;
}


struct AutoClose {
    int fd;
    AutoClose(int fd) {
        this->fd = fd;
    }
    ~AutoClose() {
        close(fd);
    }
};

static int runSocketServer() {
    vector<char> bufContainer { vl::protocol::HEADER_LEN };

    string socketAddr = config.environment.xdgRuntimeDir;
    socketAddr += "/";
    socketAddr += config.domainSocket;

    int listenFd;
    if ( (listenFd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0 ) {
        LOG_ERROR("failed to create domain socket at: ", socketAddr);
        return -1;
    }


    sockaddr_un server;

    memset(&server, 0, sizeof(server));
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, socketAddr.c_str());
    unlink(socketAddr.c_str());

    int size = offsetof(sockaddr_un, sun_path) + socketAddr.length();

    if ( bind(listenFd, (sockaddr*) &server, size) < 0 ) {
        LOG_ERROR("failed to bind domain socket: ", socketAddr);
        close(listenFd);
        return -1;
    }
    
    if ( listen(listenFd, 1) < 0 ) {
        LOG_ERROR("failed to listen domain socket: ", socketAddr);
        close(listenFd);
        return -1;
    }

    systemRunning = true;
    socketListenFd = listenFd;
    int res;
    while ( (res = acceptClient(listenFd, bufContainer)) > 0 )
        ;


    close(listenFd);
    unlink(socketAddr.c_str());
    return res;
}


bool vesperControlLive() {
    string sockAddr = config.environment.xdgRuntimeDir;
    sockAddr += '/';
    sockAddr += config.vesperCtrlSockAddr;

    if (!filesystem::exists(sockAddr)) {
        return false;  // vesper ctrl socket not detected.
    }


    // now, we should check whether vesper process presents.

    // note that if vesper quit unexpectly and new instance of vesper
    // launched without enabling vesper ctrl, this function would still
    // return true.

    string cmd = "pgrep";
    cmd += " -x -u ";
    cmd += to_string(geteuid());
    cmd += " ";
    cmd += "vesper";
    
    FILE* cmdPipe = popen(cmd.c_str(), "r");
    char buf[4];
    buf[0] = '\0';
    fgets(buf, sizeof(buf), cmdPipe);
    pclose(cmdPipe);

    return buf[0] != '\0';
}


/* ------------ 程序进入点 ------------ */

int main(int argc, const char* argv[], const char* env[]) {
    ConsoleColorPad::disableColor();

    if (int res = parseArgs(argc, argv)) {
        usage();
        return res;
    }

    ConsoleColorPad::setNoColor(userArgs.flags.contains("--no-color"));

    processEnvVars(env);

    if (processPureQueryCmds()) {
        return 0;
    }

    if (buildConfig()) {
        return -1;
    }

    if (config.quitIfVesperCtrlLive) {
        if (vesperControlLive()) {
            return 0;  // vesper ctrl detected. quit.
        }
    }

    if (config.daemonize) {
        daemonize();
    }

    if (config.serviceMode) {
        cout << "error: service mode is currently not supported!" << endl;
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    if (runSocketServer()) {
        LOG_ERROR("error occurred while running socket server!");
        return -2;
    }

    if (config.waitForChildBeforeExit) {
        int stat;
        pid_t pid = wait(&stat);
        LOG_INFO("pid ", pid, " exited, with stat: ", stat);
    }

    LOG_INFO("vesper-launcher exited successfully.");

    return 0;
}
