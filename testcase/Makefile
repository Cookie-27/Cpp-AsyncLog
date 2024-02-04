CC := g++
CFLAGS := -Wall -std=c++11
LIB_DIR := ./lib

# 静态库名称
LIB_NAME := liblog.a

# 测试程序名称
TEST_TARGET := test

# 测试程序源文件
TEST_SRCS := $(wildcard testcase/*.cpp)
TEST_OBJS := $(patsubst testcase/%.cpp, %.o, $(TEST_SRCS))

# 编译和链接测试程序
$(TEST_TARGET): $(TEST_OBJS) $(LIB_DIR)/$(LIB_NAME)
	$(CC) $(CFLAGS) $^ -o $@ -I./log -L./lib -llog

%.o: testcase/%.cpp
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(TEST_TARGET) $(TEST_OBJS)

.PHONY: clean
