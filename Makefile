CC = gcc
# Added -pthread for threading support and -D_GNU_SOURCE for dirent
CFLAGS = -Wall -Wextra -g -pthread -D_GNU_SOURCE

CLIENT_SRC = client/rfs.c
SERVER_SRC = server/rfserver.c

CLIENT_BIN = build/rfs
SERVER_BIN = build/rfserver

.PHONY: all clean

all: $(CLIENT_BIN) $(SERVER_BIN)

$(CLIENT_BIN): $(CLIENT_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^

$(SERVER_BIN): $(SERVER_SRC)
	@mkdir -p build
	$(CC) $(CFLAGS) -o $@ $^

clean:
	rm -rf build/