#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char *builtins[] = {"echo", "exit", NULL};

void command_exit(char *arguments) {
  if (arguments == NULL || arguments[0] == '\0')
    exit(0);

  int exit_code = atoi(arguments);
  exit(exit_code);
}

void command_echo(char *arguments) {
  if (arguments == NULL || arguments[0] == '\0')
    return;
  printf("%s", arguments);
}

void command_type(char *arguments) {
  if (arguments == NULL || arguments[0] == '\0')
    return;
  // check builtins first
  for (const char **cmd = builtins; *cmd != NULL; cmd++) {
    if (strcmp(*cmd, arguments) == 0) {
      printf("%s is a shell builtin", *cmd);
      return;
    }
  }
  printf("%s: not found", arguments);
}

int main() {
  char input[100];
  while (1) {
    printf("$ ");
    fflush(stdout);
    fgets(input, 100, stdin);
    input[strlen(input) - 1] = '\0';
    if (strncmp(input, "exit", sizeof("exit") - 1) == 0)
      command_exit(input + sizeof("exit"));
    else if (strncmp(input, "echo", sizeof("echo") - 1) == 0)
      command_echo(input + sizeof("echo"));
    else if (strncmp(input, "type", sizeof("type") - 1) == 0)
      command_type(input + sizeof("type"));
    else
      printf("%s: command not found", input);
    printf("\n");
  }
  return 0;
}
