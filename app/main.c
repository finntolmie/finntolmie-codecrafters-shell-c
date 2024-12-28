#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef __unix__
#define DELIM ":"
#elif defined(_WIN32) || defined(WIN32)
#define DELIM ";"
#else
#define DELIM ":"
#endif

const char *builtins[] = {"echo", "exit", "type", NULL};

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
  char *path_env = getenv("PATH");
  if (path_env == NULL) {
    printf("%s: not found", arguments);
    return;
  }
  char *path = strdup(path_env);
  char *split = strtok(path, DELIM);
  char abs_path[256];
  while (split != NULL) {
    sprintf(abs_path, "%s/%s", split, arguments);
    if (access(abs_path, F_OK) == 0 && access(abs_path, X_OK) == 0) {
      printf("%s is %s", arguments, abs_path);
      free(path);
      return;
    }
    memset(abs_path, 0, sizeof(abs_path));
    split = strtok(NULL, DELIM);
  }
  printf("%s: not found", arguments);
  free(path);
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
