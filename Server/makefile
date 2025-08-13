CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lz

SRC = main.c openServer.c createDir.c saveDir.c saveFile.c checkSum1.c ReadAndWrite.c
OBJ = $(SRC:.c=.o)

TARGET = server

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

%.o: %.c server.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) $(TARGET)
