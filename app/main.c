#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void command_exit(char *arguments) {
  if (arguments == NULL || arguments[0] == '\0')
    exit(0);

  int exit_code = atoi(arguments);
  exit(exit_code);
}

int main() {
  char input[100];
  while (1) {
    printf("$ ");
    fflush(stdout);
    fgets(input, 100, stdin);
    input[strlen(input) - 1] = '\0';
    if (strncmp(input, "exit", 4) == 0) {
      command_exit(input + sizeof("exit"));
    }
    printf("%s: command not found\n", input);
  }
  return 0;
}
