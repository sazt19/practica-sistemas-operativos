CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -D_WIN32_WINNT=0x0600 -Isrc/common
LDFLAGS = -lws2_32

SRC    = src
BIN    = bin
COMMON = $(SRC)/common

COMMON_OBJS = $(COMMON)/pipe_utils.o $(COMMON)/json_utils.o $(COMMON)/cjson/cJSON.o

TARGETS = $(BIN)/ctrllt.exe \
										$(BIN)/gesfich.exe \
										$(BIN)/gesprog.exe \
										$(BIN)/ejecutor.exe \
										$(BIN)/cliente.exe

.PHONY: all clean dirs

all: dirs $(TARGETS)

dirs:
	@mkdir -p $(BIN)
	@mkdir -p aralmac/ficheros
	@mkdir -p aralmac/programas

$(BIN)/ctrllt.exe: $(SRC)/ctrllt/main.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/gesfich.exe: $(SRC)/gesfich/main.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/gesprog.exe: $(SRC)/gesprog/main.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/ejecutor.exe: $(SRC)/ejecutor/main.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(BIN)/cliente.exe: $(SRC)/cliente/main.o $(COMMON_OBJS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(SRC)/ctrllt/main.o
	rm -f $(SRC)/gesfich/main.o
	rm -f $(SRC)/gesprog/main.o
	rm -f $(SRC)/ejecutor/main.o
	rm -f $(SRC)/cliente/main.o
	rm -f $(COMMON_OBJS)
	rm -rf $(BIN)