CC=gcc
CFLAGS=-Wall
TARGET=parse-eri-ctr-4g

all: config input output
	@$(CC) $(CFLAGS) src/main.c -o $(TARGET)
	@./$(TARGET) -r 5 -l
 
clean:
	rm $(TARGET)

config:
	mkdir $@

input:
	mkdir $@

output:
	mkdir $@
