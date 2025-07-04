/*
版权所有 (c) Mhuixs-team 2024
许可证协议:
任何人或组织在未经版权所有者同意的情况下禁止使用、修改、分发此作品
Email:hj18914255909@outlook.com

Mhuixs 数据库客户端 - C++版本
*/

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <chrono>
#include <future>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <functional>
#include <map>
#include <exception>
#include <optional>
#include <cstdint>
#include <cstring>
#include <csignal>
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <readline/readline.h>
#include <readline/history.h>

namespace mhuixs {

// 包类型枚举
enum class PacketType : uint16_t {
    QUERY = 0x0001,
    RESPONSE = 0x0002,
    ERROR = 0x0003,
    HEARTBEAT = 0x0004,
    AUTH = 0x0005,
    AUTH_RESP = 0x0006,
    DISCONNECT = 0x0007,
    KEEPALIVE = 0x0008
};

// 包状态枚举
enum class PacketStatus : uint16_t {
    OK = 0x0000,
    ERROR = 0x0001,
    PENDING = 0x0002,
    TIMEOUT = 0x0003,
    AUTH_FAILED = 0x0004,
    PERMISSION_DENIED = 0x0005,
    INVALID_QUERY = 0x0006,
    SERVER_ERROR = 0x0007
};

// 包头结构
struct PacketHeader {
    uint32_t magic{0x4D485558};      // "MHUX"
    uint16_t version{0x0100};        // 版本 1.0
    uint16_t type{0};                // 类型
    uint16_t status{0};              // 状态
    uint32_t sequence{0};            // 序列号
    uint32_t data_length{0};         // 数据长度
} __attribute__((packed));

// 数据包结构
struct Packet {
    PacketHeader header;
    std::vector<uint8_t> data;
    uint32_t checksum{0};
    
    Packet() = default;
    
    Packet(PacketType type, PacketStatus status, const std::vector<uint8_t>& packet_data = {})
        : data(packet_data) {
        header.type = static_cast<uint16_t>(type);
        header.status = static_cast<uint16_t>(status);
        header.data_length = static_cast<uint32_t>(packet_data.size());
    }
};

// 客户端异常类
class ClientException : public std::exception {
private:
    std::string message_;
    
public:
    explicit ClientException(const std::string& message) : message_(message) {}
    
    const char* what() const noexcept override {
        return message_.c_str();
    }
};

// SSL RAII 包装类
class SSLWrapper {
private:
    SSL_CTX* ctx_;
    SSL* ssl_;
    
public:
    SSLWrapper() : ctx_(nullptr), ssl_(nullptr) {
        SSL_load_error_strings();
        OpenSSL_add_ssl_algorithms();
        SSL_library_init();
        
        const SSL_METHOD* method = SSLv23_client_method();
        ctx_ = SSL_CTX_new(method);
        if (!ctx_) {
            throw ClientException("无法创建SSL上下文");
        }
        
        // 开发环境不验证证书
        SSL_CTX_set_verify(ctx_, SSL_VERIFY_NONE, nullptr);
    }
    
    ~SSLWrapper() {
        if (ssl_) {
            SSL_shutdown(ssl_);
            SSL_free(ssl_);
        }
        if (ctx_) {
            SSL_CTX_free(ctx_);
        }
        EVP_cleanup();
        ERR_free_strings();
    }
    
    SSL* create_ssl(int socket_fd) {
        ssl_ = SSL_new(ctx_);
        if (!ssl_) {
            throw ClientException("无法创建SSL连接");
        }
        
        SSL_set_fd(ssl_, socket_fd);
        
        if (SSL_connect(ssl_) <= 0) {
            SSL_free(ssl_);
            ssl_ = nullptr;
            throw ClientException("SSL连接失败");
        }
        
        return ssl_;
    }
    
    void close_ssl() {
        if (ssl_) {
            SSL_shutdown(ssl_);
            SSL_free(ssl_);
            ssl_ = nullptr;
        }
    }
};

// 主客户端类
class MhuixsClient {
private:
    std::string host_;
    int port_;
    int socket_fd_;
    std::unique_ptr<SSLWrapper> ssl_wrapper_;
    SSL* ssl_;
    bool connected_;
    bool verbose_;
    uint32_t sequence_;
    std::atomic<bool> running_;
    
    // 信号处理
    static std::function<void(int)> signal_handler_;
    
public:
    MhuixsClient(const std::string& host = "127.0.0.1", int port = 18482)
        : host_(host), port_(port), socket_fd_(-1), ssl_(nullptr), 
          connected_(false), verbose_(false), sequence_(0), running_(true) {
        
        ssl_wrapper_ = std::make_unique<SSLWrapper>();
        
        // 设置信号处理
        signal_handler_ = [this](int signum) {
            std::cout << "\n\n接收到中断信号，正在清理资源..." << std::endl;
            running_ = false;
            disconnect();
            std::exit(0);
        };
        
        std::signal(SIGINT, [](int signum) {
            if (signal_handler_) signal_handler_(signum);
        });
        
        std::signal(SIGTERM, [](int signum) {
            if (signal_handler_) signal_handler_(signum);
        });
    }
    
    ~MhuixsClient() {
        disconnect();
    }
    
    // 连接到服务器
    bool connect() {
        if (connected_) {
            std::cout << "已经连接到服务器" << std::endl;
            return true;
        }
        
        try {
            std::cout << "正在连接到 " << host_ << ":" << port_ << "..." << std::endl;
            
            // 创建套接字
            socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
            if (socket_fd_ < 0) {
                throw ClientException("创建套接字失败");
            }
            
            // 设置服务器地址
            sockaddr_in server_addr{};
            server_addr.sin_family = AF_INET;
            server_addr.sin_port = htons(port_);
            
            if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
                throw ClientException("无效的IP地址: " + host_);
            }
            
            // 连接服务器
            if (::connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
                throw ClientException("连接服务器失败");
            }
            
            // 创建SSL连接
            ssl_ = ssl_wrapper_->create_ssl(socket_fd_);
            
            connected_ = true;
            std::cout << "✓ 已成功连接到 Mhuixs 服务器" << std::endl;
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "连接失败: " << e.what() << std::endl;
            cleanup_connection();
            return false;
        }
    }
    
    // 断开连接
    void disconnect() {
        if (!connected_) return;
        
        std::cout << "正在断开连接..." << std::endl;
        connected_ = false;
        cleanup_connection();
        std::cout << "✓ 已断开连接" << std::endl;
    }
    
    // 发送查询
    bool send_query(const std::string& query) {
        if (!connected_ || !ssl_) {
            std::cerr << "错误: 未连接到服务器" << std::endl;
            return false;
        }
        
        try {
            if (verbose_) {
                std::cout << "发送查询: " << query << std::endl;
            }
            
            // 创建查询包
            std::vector<uint8_t> query_data(query.begin(), query.end());
            auto packet = create_packet(PacketType::QUERY, PacketStatus::PENDING, query_data);
            
            // 序列化并发送
            auto packet_bytes = serialize_packet(packet);
            int sent = SSL_write(ssl_, packet_bytes.data(), packet_bytes.size());
            
            if (sent <= 0) {
                throw ClientException("发送查询失败");
            }
            
            return true;
            
        } catch (const std::exception& e) {
            std::cerr << "发送查询失败: " << e.what() << std::endl;
            return false;
        }
    }
    
    // 接收响应
    std::optional<std::string> receive_response() {
        if (!connected_ || !ssl_) {
            std::cerr << "错误: 未连接到服务器" << std::endl;
            return std::nullopt;
        }
        
        try {
            // 接收包头
            std::vector<uint8_t> header_data = receive_exactly(sizeof(PacketHeader));
            if (header_data.empty()) {
                return std::nullopt;
            }
            
            // 反序列化包头
            PacketHeader header;
            std::memcpy(&header, header_data.data(), sizeof(PacketHeader));
            
            // 网络字节序转换
            header.magic = ntohl(header.magic);
            header.version = ntohs(header.version);
            header.type = ntohs(header.type);
            header.status = ntohs(header.status);
            header.sequence = ntohl(header.sequence);
            header.data_length = ntohl(header.data_length);
            
            // 验证包头
            if (header.magic != 0x4D485558) {
                throw ClientException("无效的魔数");
            }
            
            // 接收数据和校验和
            std::vector<uint8_t> remaining_data = receive_exactly(header.data_length + 4);
            if (remaining_data.empty()) {
                return std::nullopt;
            }
            
            // 提取数据
            std::vector<uint8_t> data(remaining_data.begin(), remaining_data.begin() + header.data_length);
            
            // 提取校验和
            uint32_t checksum;
            std::memcpy(&checksum, remaining_data.data() + header.data_length, 4);
            checksum = ntohl(checksum);
            
            // 验证校验和
            uint32_t calculated_checksum = calculate_checksum(header_data, data);
            if (checksum != calculated_checksum) {
                throw ClientException("校验和不匹配");
            }
            
            std::string response(data.begin(), data.end());
            
            if (verbose_) {
                std::cout << "接收响应: " << response << std::endl;
            }
            
            return response;
            
        } catch (const std::exception& e) {
            std::cerr << "接收响应失败: " << e.what() << std::endl;
            return std::nullopt;
        }
    }
    
    // 打印Logo
    void print_logo() {
        std::cout << R"(
        ███╗   ███╗██╗  ██╗██╗   ██╗██╗██╗  ██╗███████╗
        ████╗ ████║██║  ██║██║   ██║██║╚██╗██╔╝██╔════╝
        ██╔████╔██║███████║██║   ██║██║ ╚███╔╝ ███████╗
        ██║╚██╔╝██║██╔══██║██║   ██║██║ ██╔██╗ ╚════██║
        ██║ ╚═╝ ██║██║  ██║╚██████╔╝██║██╔╝ ██╗███████║
        ╚═╝     ╚═╝╚═╝  ╚═╝ ╚═════╝ ╚═╝╚═╝  ╚═╝╚══════╝
        
        Mhuixs 数据库客户端 - C++版本
        基于内存的高性能数据库
        
)" << std::endl;
    }
    
    // 打印帮助信息
    void print_help() {
        std::cout << R"(Mhuixs 客户端帮助:
  \q, \quit              退出客户端
  \h, \help              显示帮助信息
  \c, \connect           连接到服务器
  \d, \disconnect        断开连接
  \s, \status            显示连接状态
  \v, \verbose           切换详细模式
  \clear                 清屏

NAQL 查询示例:
  HOOK TABLE users;           # 创建表
  FIELD ADD name str;         # 添加字段
  ADD '张三' 25;             # 添加数据
  GET;                        # 查询所有数据
  WHERE name == '张三';       # 条件查询

)" << std::endl;
    }
    
    // 交互模式
    void interactive_mode() {
        print_logo();
        std::cout << "服务器地址: " << host_ << ":" << port_ << std::endl;
        std::cout << "输入 \\h 获取帮助信息，输入 \\q 退出" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        
        // 自动连接
        if (!connect()) {
            std::cout << "警告: 无法连接到服务器，请使用 \\c 命令手动连接" << std::endl;
        }
        
        while (running_) {
            try {
                // 设置提示符
                std::string prompt = connected_ ? "mhuixs> " : "mhuixs(断开)> ";
                
                char* input = readline(prompt.c_str());
                if (!input) {
                    std::cout << "\n再见!" << std::endl;
                    break;
                }
                
                std::string user_input = input;
                free(input);
                
                // 跳过空行
                if (user_input.empty()) {
                    continue;
                }
                
                // 添加到历史记录
                add_history(user_input.c_str());
                
                // 处理内置命令
                if (user_input[0] == '\\') {
                    handle_builtin_command(user_input);
                } else {
                    // 处理NAQL查询
                    if (connected_) {
                        if (send_query(user_input)) {
                            auto response = receive_response();
                            if (response) {
                                std::cout << *response << std::endl;
                            }
                        }
                    } else {
                        std::cout << "错误: 未连接到服务器，请使用 \\c 命令连接" << std::endl;
                    }
                }
                
            } catch (const std::exception& e) {
                std::cerr << "处理输入时发生错误: " << e.what() << std::endl;
            }
        }
        
        disconnect();
    }
    
    // 批处理模式
    void batch_mode(const std::string& filename) {
        std::ifstream file(filename);
        if (!file.is_open()) {
            std::cerr << "错误: 无法打开文件 " << filename << std::endl;
            return;
        }
        
        std::cout << "正在执行批处理文件: " << filename << std::endl;
        
        if (!connect()) {
            std::cerr << "错误: 无法连接到服务器" << std::endl;
            return;
        }
        
        std::string line;
        int line_number = 0;
        
        while (std::getline(file, line) && running_) {
            line_number++;
            
            // 去除首尾空白
            line.erase(0, line.find_first_not_of(" \t\r\n"));
            line.erase(line.find_last_not_of(" \t\r\n") + 1);
            
            // 跳过空行和注释
            if (line.empty() || line[0] == '#') {
                continue;
            }
            
            std::cout << "执行第 " << line_number << " 行: " << line << std::endl;
            
            if (send_query(line)) {
                auto response = receive_response();
                if (response) {
                    std::cout << "结果: " << *response << std::endl;
                }
            } else {
                std::cout << "查询发送失败" << std::endl;
            }
        }
        
        file.close();
        std::cout << "批处理完成" << std::endl;
    }
    
    // 设置详细模式
    void set_verbose(bool verbose) {
        verbose_ = verbose;
    }
    
    // 获取连接状态
    bool is_connected() const {
        return connected_;
    }

private:
    // 清理连接资源
    void cleanup_connection() {
        if (ssl_wrapper_) {
            ssl_wrapper_->close_ssl();
        }
        ssl_ = nullptr;
        
        if (socket_fd_ >= 0) {
            close(socket_fd_);
            socket_fd_ = -1;
        }
    }
    
    // 创建数据包
    Packet create_packet(PacketType type, PacketStatus status, const std::vector<uint8_t>& data) {
        Packet packet(type, status, data);
        packet.header.sequence = ++sequence_;
        return packet;
    }
    
    // 序列化数据包
    std::vector<uint8_t> serialize_packet(const Packet& packet) {
        std::vector<uint8_t> result;
        
        // 序列化包头（网络字节序）
        PacketHeader header = packet.header;
        header.magic = htonl(header.magic);
        header.version = htons(header.version);
        header.type = htons(header.type);
        header.status = htons(header.status);
        header.sequence = htonl(header.sequence);
        header.data_length = htonl(header.data_length);
        
        const uint8_t* header_ptr = reinterpret_cast<const uint8_t*>(&header);
        result.insert(result.end(), header_ptr, header_ptr + sizeof(PacketHeader));
        
        // 添加数据
        result.insert(result.end(), packet.data.begin(), packet.data.end());
        
        // 计算并添加校验和
        uint32_t checksum = calculate_checksum(
            std::vector<uint8_t>(result.begin(), result.begin() + sizeof(PacketHeader)),
            packet.data
        );
        checksum = htonl(checksum);
        
        const uint8_t* checksum_ptr = reinterpret_cast<const uint8_t*>(&checksum);
        result.insert(result.end(), checksum_ptr, checksum_ptr + 4);
        
        return result;
    }
    
    // 计算校验和
    uint32_t calculate_checksum(const std::vector<uint8_t>& header, const std::vector<uint8_t>& data) {
        // 简单的CRC32计算
        uint32_t crc = 0xFFFFFFFF;
        
        for (uint8_t byte : header) {
            crc ^= byte;
            for (int i = 0; i < 8; i++) {
                crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
            }
        }
        
        for (uint8_t byte : data) {
            crc ^= byte;
            for (int i = 0; i < 8; i++) {
                crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
            }
        }
        
        return crc ^ 0xFFFFFFFF;
    }
    
    // 精确接收指定字节数
    std::vector<uint8_t> receive_exactly(size_t size) {
        std::vector<uint8_t> data;
        data.reserve(size);
        
        while (data.size() < size) {
            std::vector<uint8_t> buffer(size - data.size());
            int received = SSL_read(ssl_, buffer.data(), buffer.size());
            
            if (received <= 0) {
                break;
            }
            
            data.insert(data.end(), buffer.begin(), buffer.begin() + received);
        }
        
        return data;
    }
    
    // 处理内置命令
    void handle_builtin_command(const std::string& command) {
        std::string cmd = command;
        std::transform(cmd.begin(), cmd.end(), cmd.begin(), ::tolower);
        
        if (cmd == "\\q" || cmd == "\\quit") {
            std::cout << "再见!" << std::endl;
            running_ = false;
        } else if (cmd == "\\h" || cmd == "\\help") {
            print_help();
        } else if (cmd == "\\c" || cmd == "\\connect") {
            connect();
        } else if (cmd == "\\d" || cmd == "\\disconnect") {
            disconnect();
        } else if (cmd == "\\s" || cmd == "\\status") {
            std::cout << "连接状态: " << (connected_ ? "已连接" : "未连接") << std::endl;
            if (connected_) {
                std::cout << "服务器: " << host_ << ":" << port_ << std::endl;
            }
        } else if (cmd == "\\v" || cmd == "\\verbose") {
            verbose_ = !verbose_;
            std::cout << "详细模式: " << (verbose_ ? "开启" : "关闭") << std::endl;
        } else if (cmd == "\\clear") {
            system("clear");
        } else {
            std::cout << "未知命令: " << command << std::endl;
        }
    }
};

// 静态成员初始化
std::function<void(int)> MhuixsClient::signal_handler_;

} // namespace mhuixs

// 主函数
int main(int argc, char* argv[]) {
    try {
        std::string host = "127.0.0.1";
        int port = 18482;
        std::string filename;
        bool verbose = false;
        
        // 解析命令行参数
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            
            if (arg == "-h" || arg == "--help") {
                std::cout << "Mhuixs 客户端 - C++版本\n"
                         << "用法: " << argv[0] << " [选项]\n"
                         << "选项:\n"
                         << "  -h, --help              显示此帮助信息\n"
                         << "  -s, --server <IP>       指定服务器IP地址\n"
                         << "  -p, --port <端口>       指定服务器端口\n"
                         << "  -f, --file <文件>       从文件批量执行查询\n"
                         << "  -v, --verbose           详细模式\n"
                         << "  --version               显示版本信息\n";
                return 0;
            } else if (arg == "--version") {
                std::cout << "Mhuixs 客户端 v1.0.0 - C++版本\n"
                         << "基于内存的高性能数据库客户端\n"
                         << "版权所有 (c) Mhuixs-team 2024\n";
                return 0;
            } else if (arg == "-s" || arg == "--server") {
                if (i + 1 < argc) {
                    host = argv[++i];
                } else {
                    std::cerr << "错误: 缺少服务器IP地址" << std::endl;
                    return 1;
                }
            } else if (arg == "-p" || arg == "--port") {
                if (i + 1 < argc) {
                    port = std::stoi(argv[++i]);
                } else {
                    std::cerr << "错误: 缺少端口号" << std::endl;
                    return 1;
                }
            } else if (arg == "-f" || arg == "--file") {
                if (i + 1 < argc) {
                    filename = argv[++i];
                } else {
                    std::cerr << "错误: 缺少文件名" << std::endl;
                    return 1;
                }
            } else if (arg == "-v" || arg == "--verbose") {
                verbose = true;
            } else {
                std::cerr << "错误: 未知选项 " << arg << std::endl;
                return 1;
            }
        }
        
        // 创建客户端
        mhuixs::MhuixsClient client(host, port);
        client.set_verbose(verbose);
        
        if (!filename.empty()) {
            // 批处理模式
            client.batch_mode(filename);
        } else {
            // 交互模式
            client.interactive_mode();
        }
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "客户端错误: " << e.what() << std::endl;
        return 1;
    }
}
