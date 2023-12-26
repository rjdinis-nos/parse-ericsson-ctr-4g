CC=gcc
CFLAGS=-Wall
TARGET=parse-eri-ctr-4g

all:
	@$(CC) $(CFLAGS) src/main.c -o $(TARGET)
	@./$(TARGET) -v -r 10 -d ./input -o ./output
 
clean:
	rm $(TARGET)