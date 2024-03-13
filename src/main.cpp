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

#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/un.h>

using namespace std;

const int SOCKET_DATA_BUF_SIZE = 4096;

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
} config;

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
        << endl;
}

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
    };

    for (int i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
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
    
    return 0;
}

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
static int acceptClient(int listenFd, char* buf) {

    sockaddr_un client;
    socklen_t clientLen = sizeof(client);

    int connFd = accept(listenFd, (sockaddr*) &client, &clientLen);
    if (connFd < 0) {
        LOG_WARN("socket connection failed.");
        return 1;
    }

    char* dataPtr = buf;
    int resCode = 0;
    do {
        // 读入 header

        const int HEADER_LEN = vl::protocol::HEADER_LEN;
        if (readNBytesFromSocket(connFd, HEADER_LEN, dataPtr)) {
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

        if (readNBytesFromSocket(connFd, length, dataPtr)) {
            const char* err = "failed to read body!";
            LOG_ERROR(err);
            sendResponse(connFd, 5, err);
            resCode = 5;
            break;
        }
    
        vl::protocol::Base* protocol = vl::protocol::decode(buf, type, length + HEADER_LEN);

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
    char* bufRaw = new (nothrow) char[SOCKET_DATA_BUF_SIZE];
    if (bufRaw == nullptr) {
        LOG_ERROR("failed to alloc socket data buffer");
        return -1;
    }
    unique_ptr<char> buf(bufRaw);

    string socketAddr = config.environment.xdgRuntimeDir + "/" + config.domainSocket;

    int listenFd;
    if ( (listenFd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0 ) {
        LOG_ERROR("failed to create domain socket at: ", socketAddr);
        return -1;
    }

    AutoClose autoCloseListenFd {listenFd};

    sockaddr_un server;

    memset(&server, 0, sizeof(server));
    server.sun_family = AF_UNIX;
    strcpy(server.sun_path, socketAddr.c_str());
    unlink(socketAddr.c_str());

    int size = offsetof(sockaddr_un, sun_path) + socketAddr.length();

    if ( bind(listenFd, (sockaddr*) &server, size) < 0 ) {
        LOG_ERROR("failed to bind domain socket: ", socketAddr);
        return -1;
    }
    
    if ( listen(listenFd, 1) < 0 ) {
        LOG_ERROR("failed to listen domain socket: ", socketAddr);
        return -1;
    }

    int res;
    while ( (res = acceptClient(listenFd, bufRaw)) > 0 )
        ;

    return res;
}


int main(int argc, const char* argv[], const char* env[]) {
    if (int res = parseArgs(argc, argv)) {
        usage();
        return res;
    }

    processEnvVars(env);

    if (processPureQueryCmds()) {
        return 0;
    }

    if (buildConfig()) {
        return -1;
    }

    signal(SIGPIPE, SIG_IGN);

    if (runSocketServer()) {
        LOG_ERROR("error occurred while running socket server!");
        return -2;
    }

    int stat;
    pid_t pid = wait(&stat);
    LOG_INFO("pid ", pid, " exited, with stat: ", stat);
    return 0;
}
