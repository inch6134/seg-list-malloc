CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -O3 -g

all: demo

demo: mm.o demo/main.o
	$(CC) $(CFLAGS) -o demo/demo mm.o demo/main.o

clean:
	rm -f *.o demo/*.o demo/demo


