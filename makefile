all: cwatch

cwatch: cwatch.c cwatch_lib.c
	gcc -O3 cwatch_lib.c cwatch.c -o cwatch -lm

test: cwatch test_cwatch.c cwatch_lib.c
	gcc -O3 test_cwatch.c cwatch_lib.c -o test_cwatch -lm
	./test_cwatch
	clang-format --dry-run --Werror cwatch_lib.c
	clang-format --dry-run --Werror cwatch.c
	clang-format --dry-run --Werror test_cwatch.c

clean:
	rm cwatch
