#include <stdio.h>
#include <dlfcn.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <unistd.h>
#include <cstdint>
#include <errno.h>
#include <format>
#include <regex>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>

using namespace std;

unordered_map<string, vector<string>> blacklist;
unordered_map<int, string> fd_to_filename;
string OUTPUT = getenv("OUTPUT");

bool match(const string& str, const string& pattern) {
    // replace * with .*
    string new_pattern = "";
    for (char c : pattern) {
        if (c == '*') {
            new_pattern += ".*";
        } else {
            new_pattern += c;
        }
    }
    // cout << "str: " << str << endl;
    // cout << "pattern: " << new_pattern << endl;
    regex re(new_pattern);
    // fprintf(stderr, "Match result: %d\n", regex_match(str, re));
    if (!regex_match(str, re)) {
        return false;
    }
    return true;
}

inline void logger(string msg, string output = OUTPUT) {
    if (output.empty()) {
        // cerr << "[logger] " << msg << endl;
        fprintf(stderr, "[logger] %s\n", msg.c_str());
    } else {
        ofstream file(output, ios::out | ios::app);
        file << "[logger] " << msg << endl;
        file.close();
    }
}

int writeToFile(const string& filename, const string& content) {
    ofstream file(filename, ios::out | ios::app);
    file << content;
    file.close();
    return 0;
}

string getFilenameByPath(const string& path) {
    size_t pos = path.find_last_of('/');
    return path.substr(pos + 1);
}

string trim(const string& str) {
    // trim space, \n and \r
    size_t start = 0;
    size_t end = str.size();
    while (start < end && (str[start] == ' ' || str[start] == '\n' || str[start] == '\r')) {
        start++;
    }
    while (end > start && (str[end - 1] == ' ' || str[end - 1] == '\n' || str[end - 1] == '\r')) {
        end--;
    }
    return str.substr(start, end - start);
}

void parseConfig() {
    string filename = getenv("CONFIG");
    blacklist.clear();
    blacklist["open"] = vector<string>();
    blacklist["read"] = vector<string>();
    blacklist["write"] = vector<string>();
    blacklist["connect"] = vector<string>();
    blacklist["getaddrinfo"] = vector<string>();
    blacklist["system"] = vector<string>();
    ifstream file(filename);
    string line;
    string cur_section = "";
    while (getline(file, line)) {
        if (line.empty() || line[0] == '#') {
            continue;
        }
        if (line.find("BEGIN") != string::npos) {
            cur_section = line.substr(6);
            // cout << "cur_section: " << cur_section << endl;
        } else if (line.find("END") != string::npos) {
            cur_section = "";
            // cout << "cur_section end" << endl;
        } else {
            blacklist[cur_section].push_back(trim(line));
            // cout << "blacklist[" << cur_section << "]: " << blacklist[cur_section].size() << endl;
        }
    }
}

FILE* fopen(const char* filename, const char* mode) {
    parseConfig();
    FILE* (*original_fopen)(const char*, const char*);
    original_fopen = (FILE* (*)(const char*, const char*))dlsym(RTLD_NEXT, "fopen");
    vector<string> open_blacklist = blacklist["open-blacklist"];
    char real_filename[1024] = {0};
    readlink(filename, real_filename, 1024);
    // cout << "real_filename: " << real_filename << endl;
    for (string black : open_blacklist) {
        if (match(filename, black) || match(real_filename, black)) {
            logger(format("fopen(\"{}\", \"{}\") = 0x0", filename, mode));
            errno = EACCES;
            return NULL;
        }
    }
    FILE* ret = original_fopen(filename, mode);
    fd_to_filename[fileno(ret)] = filename;
    logger(format("fopen(\"{}\", \"{}\") = 0x{:x}", filename, mode, reinterpret_cast<uintptr_t>(ret)));
    return ret;
}

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    parseConfig();
    size_t (*original_fread)(void*, size_t, size_t, FILE*);
    original_fread = (size_t (*)(void*, size_t, size_t, FILE*))dlsym(RTLD_NEXT, "fread");
    void* tmp_ptr = malloc(size * nmemb);
    size_t ret = original_fread(tmp_ptr, size, nmemb, stream);
    string filename = fd_to_filename[fileno(stream)];
    string log_filename = format("{}-{}-read.log", getpid(), getFilenameByPath(filename));
    // check content
    vector<string> read_blacklist = blacklist["read-blacklist"];
    char* buf = (char*)tmp_ptr;
    for (string black : read_blacklist) {
        // if (match(buf, black)) {
        if (string(buf).find(black) != string::npos) {
            // {pid}-{filename}-read.log
            logger(format("fread(0x{:x}, {}, {}, 0x{:x}) = 0", reinterpret_cast<uintptr_t>(tmp_ptr), size, nmemb, reinterpret_cast<uintptr_t>(stream)));
            errno = EACCES;
            return 0;
        }
    }
    writeToFile(log_filename, string(buf, ret));
    memcpy(ptr, tmp_ptr, ret);
    logger(format("fread(0x{:x}, {}, {}, 0x{:x}) = {}", reinterpret_cast<uintptr_t>(tmp_ptr), size, nmemb, reinterpret_cast<uintptr_t>(stream), ret));
    return ret;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    parseConfig();
    size_t (*original_fwrite)(const void*, size_t, size_t, FILE*);
    original_fwrite = (size_t (*)(const void*, size_t, size_t, FILE*))dlsym(RTLD_NEXT, "fwrite");
    string filename = fd_to_filename[fileno(stream)];
    string log_filename = format("{}-{}-write.log", getpid(), getFilenameByPath(filename));
    string content = string((char*)ptr);
    while (content.find('\n') != string::npos) {
        content = content.replace(content.find('\n'), 1, "\\n");
    }
    // check filename
    vector<string> write_blacklist = blacklist["write-blacklist"];
    for (string black : write_blacklist) {
        if (match(filename, black)) {
            string msg = format("fwrite(\"{}\", {}, {}, 0x{:x}) = 0", content, size, nmemb, reinterpret_cast<uintptr_t>(stream));
            logger(msg);
            errno = EACCES;
            return 0;
        }
    }
    size_t ret = original_fwrite(ptr, size, nmemb, stream);
    fprintf(stderr, "%s\n", log_filename.c_str());
    writeToFile(log_filename, string((char*)ptr, size * nmemb));
    string msg = format("fwrite(\"{}\", {}, {}, 0x{:x}) = {}", content, size, nmemb, reinterpret_cast<uintptr_t>(stream), ret);
    logger(msg);
    return ret;
}

int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen) {
    parseConfig();
    // filter by ip
    struct sockaddr_in* addr_in = (struct sockaddr_in*)addr;
    char ip[1024] = {0};
    inet_ntop(AF_INET, &addr_in->sin_addr, ip, 1024);
    string addr_str = string(ip);
    vector<string> connect_blacklist = blacklist["connect-blacklist"];
    for (string black : connect_blacklist) {
        if (match(ip, black)) {
            logger(format("connect({}, \"{}\", {}) = -1", sockfd, addr_str, addrlen));
            errno = ECONNREFUSED;
            return -1;
        }
    }
    int (*original_connect)(int, const struct sockaddr*, socklen_t);
    original_connect = (int (*)(int, const struct sockaddr*, socklen_t))dlsym(RTLD_NEXT, "connect");
    int ret = original_connect(sockfd, addr, addrlen);
    logger(format("connect({}, \"{}\", {}) = {}", sockfd, addr_str, addrlen, ret));
    return ret;
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    parseConfig();
    vector<string> getaddrinfo_blacklist = blacklist["getaddrinfo-blacklist"];
    for (string black : getaddrinfo_blacklist) {
        if (match(node, black)) {
            // logger(format("getaddrinfo(\"{}\", {}, 0x{:x}, 0x{:x}) = -1", node, service, reinterpret_cast<uintptr_t>(hints), reinterpret_cast<uintptr_t>(res)));
            if (service == NULL) {
                logger(format("getaddrinfo(\"{}\", (nil), 0x{:x}, 0x{:x}) = -1", node, reinterpret_cast<uintptr_t>(hints), reinterpret_cast<uintptr_t>(res)));
            } else {
                logger(format("getaddrinfo(\"{}\", \"{}\", 0x{:x}, 0x{:x}) = -1", node, service, reinterpret_cast<uintptr_t>(hints), reinterpret_cast<uintptr_t>(res)));
            }
            return EAI_NONAME;
        }
    }
    int (*original_getaddrinfo)(const char*, const char*, const struct addrinfo*, struct addrinfo**);
    original_getaddrinfo = (int (*)(const char*, const char*, const struct addrinfo*, struct addrinfo**))dlsym(RTLD_NEXT, "getaddrinfo");
    int ret = original_getaddrinfo(node, service, hints, res);
    if (service == NULL) {
        logger(format("getaddrinfo(\"{}\", (nil), 0x{:x}, 0x{:x}) = {}", node, reinterpret_cast<uintptr_t>(hints), reinterpret_cast<uintptr_t>(res), ret));
    } else {
        logger(format("getaddrinfo(\"{}\", \"{}\", 0x{:x}, 0x{:x}) = {}", node, service, reinterpret_cast<uintptr_t>(hints), reinterpret_cast<uintptr_t>(res), ret));
    }
    return ret;
}

int system(const char *command) {
    int (*original_system)(const char*);
    original_system = (int (*)(const char*))dlsym(RTLD_NEXT, "system");
    int ret = original_system(command);
    logger(format("system(\"{}\") = {}", command, ret), OUTPUT);
    return ret;
}