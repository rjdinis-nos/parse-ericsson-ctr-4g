CC=gcc
CFLAGS=-Wall
TARGET=parse_eri_ctr

all:
	@$(CC) $(CFLAGS) main.c -o $(TARGET)
	@./$(TARGET)
 
clean:
	rm $(TARGET)