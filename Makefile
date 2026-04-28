# CXX := g++
# CXXFLAGS := -std=c++17 -Wall -Wextra -g -Iinclude

# BUILD_DIR := build

# COMMON_SRC := src/common/socket.cc \
#               src/common/epoll.cc \
#               src/common/tcp_connection.cc

# SERVER_SRC := src/server/server.cc
# CLIENT_SRC := src/client/client.cc

# SERVER_BIN := $(BUILD_DIR)/server
# CLIENT_BIN := $(BUILD_DIR)/client

# .PHONY: all clean run-server run-client

# all: $(SERVER_BIN) $(CLIENT_BIN)

# $(BUILD_DIR):
# 	mkdir -p $(BUILD_DIR)

# $(SERVER_BIN): $(COMMON_SRC) $(SERVER_SRC) | $(BUILD_DIR)
# 	$(CXX) $(CXXFLAGS) $^ -o $@

# $(CLIENT_BIN): $(COMMON_SRC) $(CLIENT_SRC) | $(BUILD_DIR)
# 	$(CXX) $(CXXFLAGS) $^ -o $@

# run-server: $(SERVER_BIN)
# 	./$(SERVER_BIN)

# run-client: $(CLIENT_BIN)
# 	./$(CLIENT_BIN)

# clean:
# 	rm -rf $(BUILD_DIR)

CXX := g++
CXXFLAGS := -std=c++17 -Wall -Wextra -g -Iinclude

BUILD_DIR := build

COMMON_SRC := src/common/socket.cc \
              src/common/epoll.cc \
              src/common/tcp_connection.cc

SERVER_SRC := src/server/server.cc \
              src/server/server_main.cc

CLIENT_SRC := src/client/client.cc \
              src/client/client_main.cc

SERVER_BIN := $(BUILD_DIR)/server
CLIENT_BIN := $(BUILD_DIR)/client

.PHONY: all clean run-server run-client

all: $(SERVER_BIN) $(CLIENT_BIN)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(SERVER_BIN): $(COMMON_SRC) $(SERVER_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

$(CLIENT_BIN): $(COMMON_SRC) $(CLIENT_SRC) | $(BUILD_DIR)
	$(CXX) $(CXXFLAGS) $^ -o $@

run-server: $(SERVER_BIN)
	./$(SERVER_BIN)

run-client: $(CLIENT_BIN)
	./$(CLIENT_BIN)

clean:
	rm -rf $(BUILD_DIR)