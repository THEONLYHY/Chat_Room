CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g -O0 \
	-Iinclude \
	-Iinclude/common \
	-Iinclude/server \
	-Iinclude/client \
	-pthread

LDLIBS := -lspdlog -lfmt

BUILD_DIR := build

COMMON_SRCS := \
	src/common/socket.cc \
	src/common/epoll.cc \
	src/common/tcp_connection.cc \
	src/common/logger.cc \
	src/common/protocol.cc \
	src/common/user_manager.cc

SERVER_SRCS := \
	src/server/server_main.cc \
	src/server/server.cc

CLIENT_SRCS := \
	src/client/client_main.cc \
	src/client/client.cc
	
SERVER_TARGET := $(BUILD_DIR)/server
CLIENT_TARGET := $(BUILD_DIR)/client

.PHONY: all clean run-server run-client

all: $(SERVER_TARGET) $(CLIENT_TARGET)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SERVER_TARGET): $(BUILD_DIR) $(COMMON_SRCS) $(SERVER_SRCS)
	$(CXX) $(CXXFLAGS) $(COMMON_SRCS) $(SERVER_SRCS) -o $@ $(LDLIBS)

$(CLIENT_TARGET): $(BUILD_DIR) $(COMMON_SRCS) $(CLIENT_SRCS)
	$(CXX) $(CXXFLAGS) $(COMMON_SRCS) $(CLIENT_SRCS) -o $@ $(LDLIBS)

run-server: $(SERVER_TARGET)
	./$(SERVER_TARGET) 8888

run-client: $(CLIENT_TARGET)
	./$(CLIENT_TARGET) 127.0.0.1 8888

clean:
	rm -rf $(BUILD_DIR)