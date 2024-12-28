#include <stdio.h>
#include <string.h>

int main() {
  // Flush after every printf
  setbuf(stdout, NULL);

  // Wait for user input
  char input[100];
  while (1) {
    memset(input, 0, sizeof(input));
    printf("$ ");
    fgets(input, 100, stdin);
    input[strlen(input) - 1] = '\0';
    printf("%s: command not found\n", input);
  }
  return 0;
}
