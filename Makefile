CC      ?= gcc
CFLAGS  ?= -std=c11 -Wall -Wextra -O2
CPPFLAGS += -D_POSIX_C_SOURCE=200809L
LDLIBS  += -pthread -lrt

SRC := src/main.c src/util.c src/jobs.c src/logger.c src/ipc.c \
       src/banker.c src/worker.c src/limits.c
OBJ := $(SRC:.c=.o)
TARGET := campuscompute
LOG_DIR := logs
LOG_FILE := $(LOG_DIR)/campuscompute.log

.PHONY: all clean run test

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) $(OBJ) -o $@ $(LDLIBS)

src/%.o: src/%.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -pthread -c $< -o $@

run: $(TARGET)
	./$(TARGET) help

test: $(TARGET)
	./$(TARGET) schedule fcfs >/dev/null
	./$(TARGET) schedule rr 4 >/dev/null
	./$(TARGET) schedule priority >/dev/null
	./$(TARGET) compare >/dev/null
	./$(TARGET) banker both >/dev/null
	./$(TARGET) worker both >/dev/null
	./$(TARGET) limits >/dev/null
	./$(TARGET) ipc >/dev/null
	./$(TARGET) logger 2 >/dev/null

clean:
	rm -f $(OBJ) $(TARGET)
	rm -rf $(LOG_DIR)
