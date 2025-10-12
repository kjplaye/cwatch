// This is just a simple smoke test.

#include <stdio.h>

int program_main(int argc, char *argv[]);

int main(void) {
  printf("Running smoke test...\n");

  char *args[] = {"cwatch", "-i", "3", "-d", "0.1", "date", NULL};

  program_main(6, args);

  printf("Smoke test complete.\n");
  return 0;
}
