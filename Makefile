CC = cc
CFLAGS = -Wall -Wextra -Wpedantic -O3 -g

all: demo

demo: mm.o demo/main.o demo/implicit.o demo/explicit.o
	$(CC) $(CFLAGS) -o demo.out mm.o demo/main.o demo/implicit.o demo/explicit.o

clean:
	rm -f *.o demo/*.o demo.out


