all: cwatch

cwatch: cwatch.c
	gcc -O3 cwatch.c -o cwatch -lm

clean:
	rm cwatch
