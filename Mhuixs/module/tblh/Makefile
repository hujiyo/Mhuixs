# 定义目标可执行文件
TARGET = hj.exe

# 定义所有的源文件
SRC_FILES = tblhbase.cpp tblhop.cpp tblhschama.cpp tblh.cpp ../../mlib/time.cpp

# 定义所有的目标文件（.o 文件），通过将源文件的后缀名 .cpp 替换为 .o 得到
OBJ_FILES = $(patsubst %.cpp, %.o, $(SRC_FILES))

# C++ 编译器
CXX = g++

# 编译器选项，-Wall 表示开启所有警告，-g 表示生成调试信息
CXXFLAGS = -Wall -g

# 链接器选项
LDFLAGS = 

# 构建目标可执行文件的规则
$(TARGET): $(OBJ_FILES)
	$(CXX) $(LDFLAGS) $^ -o $@

# 构建每个 .o 文件的规则，$< 表示第一个依赖文件（即对应的 .cpp 文件）
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# 清理规则，删除所有的 .o 文件和目标可执行文件
clean:
	rm -f $(OBJ_FILES) $(TARGET)