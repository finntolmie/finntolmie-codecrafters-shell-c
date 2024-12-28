#include <stdio.h>
#include <string.h>

int main() {
  char input[100];
  while (1) {
    printf("$ ");
    fflush(stdout);
    fgets(input, 100, stdin);
    input[strlen(input) - 1] = '\0';
    printf("%s: command not found\n", input);
  }
  return 0;
}
