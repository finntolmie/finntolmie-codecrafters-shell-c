#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#ifdef __unix__
#define DELIM ":"
#elif defined(_WIN32) || defined(WIN32)
#define DELIM ";"
#else
#define DELIM ":"
#endif

const char *builtins[] = {"echo", "exit", "type", NULL};

int get_executable(char *dst, char *path, char *fname) {
  char *split = strtok(path, DELIM);
  while (split != NULL) {
    sprintf(dst, "%s/%s", split, fname);
    if (access(dst, F_OK) == 0 && access(dst, X_OK) == 0) {
      return 1;
    }
    split = strtok(NULL, DELIM);
  }
  return 0;
}

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

void command_type(char *type_args) {
  if (type_args == NULL || type_args[0] == '\0')
    return;
  for (const char **cmd = builtins; *cmd != NULL; cmd++) {
    if (strcmp(*cmd, type_args) == 0) {
      printf("%s is a shell builtin", *cmd);
      return;
    }
  }

  char *exe_name = strtok(type_args, " ");
  char *exe_args = strtok(NULL, " ");

  char *path_env = getenv("PATH");
  if (path_env == NULL) {
    printf("%s: not found", exe_name);
    return;
  }

  char abs_path[256];
  char *path = strdup(path_env);
  int found = get_executable(abs_path, path, exe_name);
  free(path);
  if (found)
    printf("%s is %s", exe_name, abs_path);
  else
    printf("%s: not found", exe_name);
}

void command_execute(char *arguments) {
  char *exe_name = strtok(arguments, " ");
  char *exe_args = strtok(NULL, " ");

  char *path_env = getenv("PATH");
  if (path_env == NULL) {
    printf("%s: not found\n", exe_name);
    return;
  }

  char abs_path[256];
  char *path = strdup(path_env);
  int found = get_executable(abs_path, path, exe_name);
  free(path);

  size_t argc = 1;
  const char *argi = exe_args;
  while (argi != NULL && *argi != '\0') {
    if (*argi == ' ')
      argc++;
    argi++;
  }

  char **argv = malloc((argc + 1) * sizeof(char *));
  if (argv == NULL) {
    perror("malloc");
    return;
  }

  argv[0] = exe_name;

  char *tok = strtok(exe_args, " ");
  int i;
  for (i = 1; i < argc + 1 && tok != NULL; i++) {
    argv[i] = tok;
    tok = strtok(NULL, " ");
  }
  argv[i] = NULL;

  pid_t pid = fork();
  if (pid == 0) {
    execv(abs_path, argv);
    perror("execv");
    free(argv);
    return;
  } else if (pid == -1) {
    perror("fork");
    free(argv);
    return;
  } else {
    int status;
    waitpid(pid, &status, 0);
  }
  free(argv);
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
    else {
      command_execute(input);
      continue;
    }
    printf("\n");
  }
  return 0;
}
