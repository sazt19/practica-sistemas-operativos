CC      = gcc
CFLAGS  = -Wall -Wextra -std=c11 -g -D_WIN32_WINNT=0x0600
LDFLAGS = -lws2_32

# Directorios
SRC     = src
BIN     = bin
COMMON  = $(SRC)/common

# Fuentes comunes
COMMON_SRCS = $(COMMON)/pipe_utils.c $(COMMON)/json_utils.c $(COMMON)/cjson/cJSON.c
COMMON_OBJS = $(COMMON_SRCS:.c=.o)

# Targets
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

$(BIN)/ctrllt.exe: $(SRC)/ctrllt/main.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -I$(COMMON) $^ -o $@ $(LDFLAGS)

$(BIN)/gesfich.exe: $(SRC)/gesfich/main.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -I$(COMMON) $^ -o $@ $(LDFLAGS)

$(BIN)/gesprog.exe: $(SRC)/gesprog/main.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -I$(COMMON) $^ -o $@ $(LDFLAGS)

$(BIN)/ejecutor.exe: $(SRC)/ejecutor/main.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -I$(COMMON) $^ -o $@ $(LDFLAGS)

$(BIN)/cliente.exe: $(SRC)/cliente/main.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -I$(COMMON) $^ -o $@ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -I$(COMMON) -c $< -o $@

clean:
	rm -f $(COMMON_OBJS) $(TARGETS)
	rm -rf $(BIN)
