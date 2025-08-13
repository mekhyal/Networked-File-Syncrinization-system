CC = gcc
CFLAGS = -Wall -g
LDFLAGS = -lz

SRC = main.c openClient.c traversDir.c fileToSocket.c checksum.c dirToSocket.c ReadAndWrite.c
OBJ = $(SRC:.c=.o)

TARGET = client

all: $(TARGET)

$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ) $(LDFLAGS)

%.o: %.c client.h
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f $(OBJ) $(TARGET)   
	
