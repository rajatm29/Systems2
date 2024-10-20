# Makefile for building yash client and yashd server

# Compiler and flags
CC       = gcc
CFLAGS   = -Wall -Wextra -pedantic -std=c11
PTHREAD  = -pthread

# Executable names
CLIENT   = yash
SERVER   = yashd

# Source files
CLIENT_SRC = yash.c
SERVER_SRC = yashd.c

# Header files
CLIENT_HDR = yash.h
SERVER_HDR = yashd.h

# Object files
CLIENT_OBJ = $(CLIENT_SRC:.c=.o)
SERVER_OBJ = $(SERVER_SRC:.c=.o)

# Default target
all: $(CLIENT) $(SERVER)

# Build the client
$(CLIENT): $(CLIENT_OBJ)
	$(CC) $(CFLAGS) -o $@ $^

# Build the server
$(SERVER): $(SERVER_OBJ)
	$(CC) $(CFLAGS) $(PTHREAD) -o $@ $^

# Compile client source files
$(CLIENT_OBJ): $(CLIENT_SRC) $(CLIENT_HDR)
	$(CC) $(CFLAGS) -c $(CLIENT_SRC)

# Compile server source files
$(SERVER_OBJ): $(SERVER_SRC) $(SERVER_HDR)
	$(CC) $(CFLAGS) $(PTHREAD) -c $(SERVER_SRC)

# Clean up generated files
clean:
	rm -f $(CLIENT) $(SERVER) $(CLIENT_OBJ) $(SERVER_OBJ)

# Restart the daemon (for development)
restart:
	# Stop the existing daemon if running
	pkill $(SERVER) || true
	# Rebuild the server
	make clean
	make $(SERVER)
	# Start the daemon
	./$(SERVER)

# Phony targets
.PHONY: all clean restart
