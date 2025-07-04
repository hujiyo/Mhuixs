#!/bin/bash
# Mhuixs 客户端构建脚本
# 版权所有 (c) Mhuixs-team 2024

set -e  # 遇到错误时退出

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# 函数定义
print_header() {
    echo -e "${BLUE}================================================${NC}"
    echo -e "${BLUE}$1${NC}"
    echo -e "${BLUE}================================================${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_info() {
    echo -e "${BLUE}ℹ $1${NC}"
}

# 检查依赖
check_dependencies() {
    print_header "检查构建依赖"
    
    # 检查基本工具
    local missing_deps=()
    
    if ! command -v gcc &> /dev/null; then
        missing_deps+=("gcc")
    fi
    
    if ! command -v g++ &> /dev/null; then
        missing_deps+=("g++")
    fi
    
    if ! command -v cmake &> /dev/null; then
        missing_deps+=("cmake")
    fi
    
    if ! command -v python3 &> /dev/null; then
        missing_deps+=("python3")
    fi
    
    if ! command -v make &> /dev/null; then
        missing_deps+=("make")
    fi
    
    # 检查开发库
    if ! pkg-config --exists openssl; then
        missing_deps+=("libssl-dev")
    fi
    
    if ! pkg-config --exists readline; then
        missing_deps+=("libreadline-dev")
    fi
    
    if [ ${#missing_deps[@]} -ne 0 ]; then
        print_error "缺少以下依赖: ${missing_deps[*]}"
        print_info "Ubuntu/Debian 安装命令:"
        echo "  sudo apt-get install build-essential cmake python3 libssl-dev libreadline-dev"
        print_info "CentOS/RHEL 安装命令:"
        echo "  sudo yum install gcc gcc-c++ cmake python3 openssl-devel readline-devel"
        exit 1
    fi
    
    print_success "所有依赖检查通过"
}

# 构建C版本
build_c_client() {
    print_header "构建 C 版本客户端"
    
    cd C
    
    if [ ! -f Makefile ]; then
        print_error "未找到 Makefile"
        cd ..
        return 1
    fi
    
    print_info "清理旧的构建文件..."
    make clean &> /dev/null || true
    
    print_info "开始编译..."
    if make all; then
        print_success "C版本客户端构建成功"
        
        if [ -f mhuixs-client ]; then
            print_info "可执行文件: $(pwd)/mhuixs-client"
        fi
    else
        print_error "C版本客户端构建失败"
        cd ..
        return 1
    fi
    
    cd ..
}

# 构建C++版本
build_cpp_client() {
    print_header "构建 C++ 版本客户端"
    
    cd Cpp
    
    if [ ! -f CMakeLists.txt ]; then
        print_error "未找到 CMakeLists.txt"
        cd ..
        return 1
    fi
    
    # 创建构建目录
    mkdir -p build
    cd build
    
    print_info "配置CMake..."
    if cmake -DCMAKE_BUILD_TYPE=Release ..; then
        print_info "开始编译..."
        if make -j$(nproc); then
            print_success "C++版本客户端构建成功"
            
            if [ -f bin/mhuixs-client-cpp ]; then
                print_info "可执行文件: $(pwd)/bin/mhuixs-client-cpp"
            fi
        else
            print_error "C++版本客户端构建失败"
            cd ../..
            return 1
        fi
    else
        print_error "CMake配置失败"
        cd ../..
        return 1
    fi
    
    cd ../..
}

# 准备Python版本
prepare_python_client() {
    print_header "准备 Python 版本客户端"
    
    cd Python
    
    if [ ! -f clt.py ]; then
        print_error "未找到 clt.py"
        cd ..
        return 1
    fi
    
    # 检查Python版本
    python_version=$(python3 -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')")
    print_info "Python版本: $python_version"
    
    if [ ! -f requirements.txt ]; then
        print_warning "未找到 requirements.txt，跳过依赖安装"
    else
        print_info "Python客户端使用标准库，无需安装额外依赖"
    fi
    
    # 设置执行权限
    chmod +x clt.py
    
    print_success "Python版本客户端准备完成"
    print_info "可执行文件: $(pwd)/clt.py"
    
    cd ..
}

# 运行测试
run_tests() {
    print_header "运行测试"
    
    # 测试C版本
    if [ -f C/mhuixs-client ]; then
        print_info "测试C版本客户端..."
        if C/mhuixs-client --help &> /dev/null; then
            print_success "C版本客户端测试通过"
        else
            print_warning "C版本客户端测试失败"
        fi
    fi
    
    # 测试C++版本
    if [ -f Cpp/build/bin/mhuixs-client-cpp ]; then
        print_info "测试C++版本客户端..."
        if Cpp/build/bin/mhuixs-client-cpp --help &> /dev/null; then
            print_success "C++版本客户端测试通过"
        else
            print_warning "C++版本客户端测试失败"
        fi
    fi
    
    # 测试Python版本
    if [ -f Python/clt.py ]; then
        print_info "测试Python版本客户端..."
        if python3 Python/clt.py --help &> /dev/null; then
            print_success "Python版本客户端测试通过"
        else
            print_warning "Python版本客户端测试失败"
        fi
    fi
}

# 显示使用说明
show_usage() {
    print_header "使用说明"
    
    echo "构建完成后的客户端位置："
    echo ""
    
    if [ -f C/mhuixs-client ]; then
        echo "  C版本:      ./C/mhuixs-client"
    fi
    
    if [ -f Cpp/build/bin/mhuixs-client-cpp ]; then
        echo "  C++版本:    ./Cpp/build/bin/mhuixs-client-cpp"
    fi
    
    if [ -f Python/clt.py ]; then
        echo "  Python版本: python3 ./Python/clt.py"
    fi
    
    echo ""
    echo "使用示例："
    echo "  ./C/mhuixs-client                        # 启动C版本客户端"
    echo "  ./C/mhuixs-client -s 192.168.1.100      # 连接到指定服务器"
    echo "  python3 ./Python/clt.py --help          # 查看Python版本帮助"
    echo "  ./Cpp/build/bin/mhuixs-client-cpp -v     # 启动C++版本（详细模式）"
    echo ""
}

# 清理函数
clean_all() {
    print_header "清理所有构建文件"
    
    # 清理C版本
    if [ -d C ]; then
        cd C
        make clean &> /dev/null || true
        cd ..
        print_success "清理C版本构建文件"
    fi
    
    # 清理C++版本
    if [ -d Cpp/build ]; then
        rm -rf Cpp/build
        print_success "清理C++版本构建文件"
    fi
    
    print_success "清理完成"
}

# 主函数
main() {
    cd "$(dirname "$0")"
    
    print_header "Mhuixs 客户端构建脚本"
    
    case "${1:-all}" in
        "deps"|"check-deps")
            check_dependencies
            ;;
        "c")
            check_dependencies
            build_c_client
            ;;
        "cpp"|"c++")
            check_dependencies
            build_cpp_client
            ;;
        "python"|"py")
            check_dependencies
            prepare_python_client
            ;;
        "test")
            run_tests
            ;;
        "clean")
            clean_all
            ;;
        "all")
            check_dependencies
            
            # 构建所有版本
            build_c_client || print_warning "C版本构建失败，继续构建其他版本"
            build_cpp_client || print_warning "C++版本构建失败，继续构建其他版本"
            prepare_python_client || print_warning "Python版本准备失败"
            
            run_tests
            show_usage
            ;;
        "help"|"-h"|"--help")
            echo "Mhuixs 客户端构建脚本"
            echo ""
            echo "用法: $0 [选项]"
            echo ""
            echo "选项:"
            echo "  all        构建所有版本的客户端（默认）"
            echo "  c          仅构建C版本客户端"
            echo "  cpp|c++    仅构建C++版本客户端"
            echo "  python|py  仅准备Python版本客户端"
            echo "  deps       检查构建依赖"
            echo "  test       运行测试"
            echo "  clean      清理所有构建文件"
            echo "  help       显示此帮助信息"
            ;;
        *)
            print_error "未知选项: $1"
            echo "使用 '$0 help' 查看帮助信息"
            exit 1
            ;;
    esac
}

# 执行主函数
main "$@" 