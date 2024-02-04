CC := g++
CFLAGS := -Wall -std=c++11
LOG_DIR := log
LIB_DIR := lib

# 源文件和目标文件
LOG_SRCS := $(wildcard $(LOG_DIR)/*.cpp)
LOG_OBJS := $(patsubst $(LOG_DIR)/%.cpp, %.o, $(LOG_SRCS))

# 静态库名称
LIB_NAME := liblog.a

# 生成静态库
lib: $(LOG_OBJS)
	@mkdir -p $(LIB_DIR)
	ar rcs $(LIB_DIR)/$(LIB_NAME) $^

%.o: $(LOG_DIR)/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(LIB_DIR)

.PHONY: lib clean
