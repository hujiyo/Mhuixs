# Mhuixs C++客户端 CMakeLists.txt
# 版权所有 (c) Mhuixs-team 2024

cmake_minimum_required(VERSION 3.12)
project(mhuixs-client-cpp VERSION 1.0.0 LANGUAGES CXX)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# 编译选项
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 -DDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# 默认构建类型
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# 寻找依赖包
find_package(OpenSSL REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(READLINE REQUIRED readline)

# 源文件
set(SOURCES
    clt.cpp
)

# 可执行文件
add_executable(mhuixs-client-cpp ${SOURCES})

# 编译选项
target_compile_options(mhuixs-client-cpp PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -Wformat=2
    -Wcast-align
    -Wcast-qual
    -Wctor-dtor-privacy
    -Wdisabled-optimization
    -Wlogical-op
    -Wmissing-include-dirs
    -Wnoexcept
    -Wold-style-cast
    -Woverloaded-virtual
    -Wredundant-decls
    -Wshadow
    -Wsign-promo
    -Wstrict-null-sentinel
    -Wundef
    -Wno-unused
)

# 包含目录
target_include_directories(mhuixs-client-cpp PRIVATE
    ${OPENSSL_INCLUDE_DIR}
    ${READLINE_INCLUDE_DIRS}
)

# 链接库
target_link_libraries(mhuixs-client-cpp
    ${OPENSSL_LIBRARIES}
    ${READLINE_LIBRARIES}
    pthread
)

# 设置输出目录
set_target_properties(mhuixs-client-cpp PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# 安装规则
install(TARGETS mhuixs-client-cpp
    RUNTIME DESTINATION bin
)

# 测试
enable_testing()

# 简单的连接测试
add_test(NAME client_help_test
    COMMAND mhuixs-client-cpp --help
)

add_test(NAME client_version_test
    COMMAND mhuixs-client-cpp --version
)

# 自定义目标
add_custom_target(run
    COMMAND ${CMAKE_BINARY_DIR}/bin/mhuixs-client-cpp
    DEPENDS mhuixs-client-cpp
    COMMENT "运行Mhuixs C++客户端"
)

add_custom_target(debug_run
    COMMAND gdb ${CMAKE_BINARY_DIR}/bin/mhuixs-client-cpp
    DEPENDS mhuixs-client-cpp
    COMMENT "使用gdb调试Mhuixs C++客户端"
)

# 打包
set(CPACK_PACKAGE_NAME "mhuixs-client-cpp")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Mhuixs数据库C++客户端")
set(CPACK_PACKAGE_VENDOR "Mhuixs Team")
set(CPACK_PACKAGE_CONTACT "hj18914255909@outlook.com")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/../../../LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/../../../README.md")

# DEB包设置
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libssl1.1, libreadline8")
set(CPACK_DEBIAN_PACKAGE_SECTION "database")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")

# RPM包设置
set(CPACK_RPM_PACKAGE_REQUIRES "openssl-libs, readline")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Databases")

include(CPack)

# 显示构建信息
message(STATUS "Mhuixs C++客户端构建配置:")
message(STATUS "  构建类型: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++标准: ${CMAKE_CXX_STANDARD}")
message(STATUS "  编译器: ${CMAKE_CXX_COMPILER}")
message(STATUS "  OpenSSL版本: ${OPENSSL_VERSION}")
message(STATUS "  安装前缀: ${CMAKE_INSTALL_PREFIX}") 