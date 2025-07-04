#include "env.hpp"

// 获取系统物理内存（MB）
static int get_system_memory_mb() {
#ifdef _WIN32
    MEMORYSTATUSEX statex;
    statex.dwLength = sizeof(statex);
    if (GlobalMemoryStatusEx(&statex)) {
        return (int)(statex.ullTotalPhys / (1024 * 1024));
    }
    cerr << "[env] 获取系统内存信息失败，使用默认值4096MB。" << endl;
    return 4096;
#else
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (int)(info.totalram * info.mem_unit / (1024 * 1024));
    }
    cerr << "[env] 获取系统内存信息失败，使用默认值4096MB。" << endl;
    return 4096;
#endif
}

// 检查目录是否可用
static bool check_dir_available(const std::string& path) {
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

// 全局唯一配置变量
ENV Env = {"", 0, 0};

// 获取当前程序所在目录，拼接配置文件名（C++ string实现）
static string get_config_path() {
    char exe_path[ENV_PATH_MAX];
#ifdef _WIN32    
    DWORD len = GetModuleFileNameA(NULL, exe_path, ENV_PATH_MAX);
    if (len == 0 || len == ENV_PATH_MAX) return "Mhuixs.config";
    char* last_slash = strrchr(exe_path, '\\');
    if (!last_slash) return "Mhuixs.config";
    string dir(exe_path, last_slash + 1 - exe_path);
    return dir + "Mhuixs.config";
#else
    ssize_t len = readlink("/proc/self/exe", exe_path, ENV_PATH_MAX - 1);
    if (len <= 0 || len >= ENV_PATH_MAX) return string("Mhuixs.config");
    exe_path[len] = '\0';
    char* last_slash = strrchr(exe_path, '/');
    if (!last_slash) return string("Mhuixs.config");
    string dir(exe_path, last_slash + 1 - exe_path);
    return dir + "Mhuixs.config";
#endif
}

// 辅助函数：去除首尾空白
static string trim(const string& s) {
    size_t start = 0, end = s.size();
    while (start < end && isspace((unsigned char)s[start])) ++start;
    while (end > start && isspace((unsigned char)s[end-1])) --end;
    return s.substr(start, end - start);
}

// 解析空格分隔的配置文件
int env_init() {
    string config_path = get_config_path();
    FILE* fp = fopen(config_path.c_str(), "r");
    if (!fp) {
        cerr << "[env] 配置文件不存在: " << config_path << endl;
        return 1;
    }
    string MhuixsHomePath;
    int threadslimit = 0, memmorylimit = 0;
    char line[512];
    int found_datapath = 0, found_threadslimit = 0, found_memmorylimit = 0;
    while (fgets(line, sizeof(line), fp)) {
        string strline = trim(line);
        if (strline.empty() || strline[0] == '#') continue;
        istringstream iss(strline);
        string key, value;
        if (!(iss >> key >> value)) continue;
        key = trim(key); value = trim(value);
        if (key == "MhuixsHomePath") {
            MhuixsHomePath = value;
            found_datapath = 1;
        } else if (key == "threadslimit") {
            try {
                int v = stoi(value);
                if (v >= 2) { threadslimit = v; found_threadslimit = 1; }
                else {
                    cerr << "[env] 线程数过小(<2)，已忽略配置，采用默认值。" << endl;
                }
            } catch (...) {
                cerr << "[env] 线程数配置非法，已忽略配置，采用默认值。" << endl;
            }
        } else if (key == "memmorylimit") {
            try {
                int v = stoi(value);
                if (v >= 64) { memmorylimit = v; found_memmorylimit = 1; }
                else {
                    cerr << "[env] 内存限制过小(<64MB)，已忽略配置，采用默认值。" << endl;
                }
            } catch (...) {
                cerr << "[env] 内存限制配置非法，已忽略配置，采用默认值。" << endl;
            }
        }
    }
    fclose(fp);
    // 检查数据目录
    if (!found_datapath || !check_dir_available(MhuixsHomePath)) {
        cerr << "[env] 数据目录不可用: " << MhuixsHomePath << endl;
        return 1;
    }
    // 系统内存限制
    int sys_mem = get_system_memory_mb();
    int mem_limit = memmorylimit;
    int mem_max = sys_mem * 90 / 100;
    int mem_default = sys_mem * 75 / 100;
    if (!found_memmorylimit || mem_limit < 64 || mem_limit > mem_max) {
        cerr << "[env] 内存限制配置不合法或超出系统上限，将采用系统内存的75%: " << mem_default << "MB" << endl;
        mem_limit = mem_default;
    }
    // 线程数
    int thread_limit = threadslimit;
    if (!found_threadslimit || thread_limit < 2) {
        cerr << "[env] 线程数配置不合法(<2)，将采用默认值2。" << endl;
        thread_limit = 2;
    }
    Env.MhuixsHomePath = MhuixsHomePath;
    Env.threadslimit = thread_limit;
    Env.memmorylimit = mem_limit;
    return 0;
}
