#include "env.hpp"

ENV Env = {"", 0, 0};

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
#else //Linux
    struct sysinfo info;
    if (sysinfo(&info) == 0) {
        return (int)(info.totalram * info.mem_unit / (1024 * 1024));
    }
    cerr << "[env] 获取系统内存信息失败，使用默认值4096MB。" << endl;
    return 4096;
#endif
}

static bool check_dir_available(const std::string& path) {
#ifdef _WIN32
    struct _stat st;
    return _stat(path.c_str(), &st) == 0 && (st.st_mode & _S_IFDIR);
#else
    struct stat st;
    return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
#endif
}

// 校验目录存在性
static string parse_path(const string& value, const string& warnmsg) {
    if (check_dir_available(value)) return value;
    cerr << warnmsg << ": " << value << endl;
    return "";
}

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
#else //Linux
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
    int sys_mem = get_system_memory_mb();
    int mem_max = sys_mem * 90 / 100;
    int mem_default = sys_mem * 75 / 100;
    while (fgets(line, sizeof(line), fp)) {
        string strline = trim(line);
        if (strline.empty() || strline[0] == '#') continue;
        istringstream iss(strline);
        string key, value;
        if (!(iss >> key >> value)) continue;
        key = trim(key); value = trim(value);
        if (key == "MhuixsHomePath") {
            MhuixsHomePath = parse_path(value, "[env] 数据目录不可用");
            found_datapath = !MhuixsHomePath.empty();
        } else if (key == "threadslimit") {
            threadslimit = parse_int(value, 2, 1024, 2, "[env] 线程数配置非法(<2)");
            found_threadslimit = 1;
        } else if (key == "memmorylimit") {
            memmorylimit = parse_int(value, 64, mem_max, mem_default, "[env] 内存限制配置非法(<64MB)");
            found_memmorylimit = 1;
        }
        // 以后可在此添加更多变量解析
    }
    fclose(fp);
    if (!found_datapath) {
        cerr << "[env] 数据目录不可用: " << MhuixsHomePath << endl;
        return 1;
    }
    if (!found_memmorylimit || memmorylimit < 64 || memmorylimit > mem_max) {
        cerr << "[env] 内存限制配置不合法或超出系统上限，将采用系统内存的75%: " << mem_default << "MB" << endl;
        memmorylimit = mem_default;
    }
    if (!found_threadslimit || threadslimit < 2) {
        cerr << "[env] 线程数配置不合法(<2)，将采用默认值2。" << endl;
        threadslimit = 2;
    }
    Env.MhuixsHomePath = MhuixsHomePath;
    Env.threadslimit = threadslimit;
    Env.memmorylimit = memmorylimit;
    return 0;
}
