# Makefile for building yash (client) and yashd (server)

# Compiler
CC = gcc

# Compiler flags
CFLAGS = -Wall -Wextra -g

# Target executables
CLIENT = yash
SERVER = yashd

# Source files
CLIENT_SRC = yash.c
SERVER_SRC = yashd.c

# Build rules
all: $(CLIENT) $(SERVER)

$(CLIENT): $(CLIENT_SRC)
	$(CC) $(CFLAGS) -o $(CLIENT) $(CLIENT_SRC)

$(SERVER): $(SERVER_SRC)
	$(CC) $(CFLAGS) -o $(SERVER) $(SERVER_SRC) -lpthread

# Clean rule to remove compiled binaries
clean:
	rm -f $(CLIENT) $(SERVER)

# Phony targets
.PHONY: all clean
