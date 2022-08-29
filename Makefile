#
# Author: 383494
#

BIN = main
CC = gcc
CFLAGS = -Wall -lcurl -lcjson -g
RM = rm
OBJ = main.o miraiBot.o

main: $(OBJ)
	$(CC) $(OBJ) $(CFLAGS) -o $(BIN)
main.o: main.c miraiBot.h
	$(CC) -c main.c $(CFLAGS) -o main.o
miraiBot.o: miraiBot.c miraiBot.h
	$(CC) -c miraiBot.c $(CFLAGS) -o miraiBot.o

.PHONY: clean
clean:
	$(RM) $(OBJ) $(BIN)
