#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
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

#define BUFFER_SIZE 256

const char *builtins[] = {"echo", "exit", "type", "pwd", "cd", NULL};

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

int is_executable(char *path) { return access(path, X_OK) == 0; }

char *find_in_path(const char *cmd) {
  char *path_env = getenv("PATH");

  if (path_env == NULL)
    return NULL;

  char *path_copy = strdup(path_env);
  char *dir = strtok(path_copy, DELIM);

  static char full_path[1024];
  while (dir != NULL) {
    snprintf(full_path, sizeof(full_path), "%s/%s", dir, cmd);
    if (is_executable(full_path)) {
      free(path_copy);
      return full_path;
    }
    dir = strtok(NULL, DELIM);
  }
  free(path_copy);
  return NULL;
}

// args could look like:
// arg1 arg2 arg3 => 3 args
// or
// arg1 'arg 2' arg3 => 3 args
// or
// 'arg1 arg2 arg3' => 1 arg
// will return a string array with the last element being NULL
char **parse_args(const char *args) {
  if (args == NULL)
    return NULL;

  size_t buf_size = 10;
  size_t argc = 0;
  char **argv = malloc(buf_size * sizeof(char *));
  if (argv == NULL) {
    perror("malloc");
    return NULL;
  }

  const char *ptr = args;
  while (*ptr != '\0' && argc < buf_size) {
    while (isspace((unsigned char)*ptr))
      ptr++;
    if (*ptr == '\0')
      break;

    char quote_char = '\0';
    if (*ptr == '\'' || *ptr == '"') {
      quote_char = *ptr;
      ptr++;
    }

    const char *start = ptr;
    char *arg = malloc(1024);
    if (arg == NULL) {
      perror("malloc");
      goto parse_error;
    }
    size_t arglen = 0;
    while (*ptr != '\0' && ((quote_char && *ptr != quote_char) ||
                            (!quote_char && !isspace((unsigned char)*ptr)))) {
      if (*ptr == '\\' && !quote_char) {
        ptr++;
        if (*ptr == '\0')
          break;
      }
      arg[arglen++] = *ptr++;
    }

    if (quote_char && *ptr == quote_char)
      ptr++;

    arg = realloc(arg, arglen + 1);
    if (arg == NULL) {
      perror("realloc");
      goto parse_error;
    }
    arg[arglen] = '\0';
    argv[argc++] = arg;
  }
  argv[argc] = NULL;
  return argv;

parse_error:
  for (size_t i = 0; i < argc; i++)
    free(argv[i]);
  free(argv);
  return NULL;
}

void exec_cmd(char *abs_path, char **argv) {
  pid_t pid = fork();
  if (pid == 0) {
    execv(abs_path, argv);
    perror("execv");
    exit(1);
  } else if (pid == -1) {
    perror("fork");
  } else {
    int status;
    if (waitpid(pid, &status, 0) == -1)
      perror("waitpid");
  }
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
  char **argv = parse_args(arguments);
  int first = 1;
  for (char **args = argv; *args != NULL; args++) {
    if (!first)
      printf(" ");
    printf("%s", *args);
    free(*args);
    first = 0;
  }
  printf("\n");
  free(argv);
}

void command_cd(char *arguments) {
  if (arguments[0] == '~') {
    char *home = getenv("HOME");
    if (home == NULL) {
      printf("cd: %s: No such file or directory\n", arguments);
      return;
    }
    int homelen = strlen(home);
    int arglen = strlen(arguments);
    int size = homelen + arglen - 1;
    if (size >= BUFFER_SIZE) {
      printf("cd: %s: No such file or directory\n", arguments);
      return;
    }
    memmove(arguments + homelen, arguments + 1, arglen);
    memcpy(arguments, home, homelen);
    arguments[homelen + arglen - 1] = '\0';
  }
  struct stat sb;
  if (stat(arguments, &sb) == 0 && S_ISDIR(sb.st_mode)) {
    chdir(arguments);
  } else {
    printf("cd: %s: No such file or directory\n", arguments);
  }
}

void command_type(char *type_args) {
  if (type_args == NULL || type_args[0] == '\0')
    return;
  for (const char **cmd = builtins; *cmd != NULL; cmd++) {
    if (strcmp(*cmd, type_args) == 0) {
      printf("%s is a shell builtin\n", *cmd);
      return;
    }
  }
  char *cmd = strtok(type_args, " ");
  char *path = find_in_path(cmd);
  if (path)
    printf("%s is %s\n", cmd, path);
  else
    printf("%s: not found\n", cmd);
}

void command_execute(char *arguments) {
  char **argv = parse_args(arguments);
  char *cmd_path = find_in_path(argv[0]);
  if (cmd_path) {
    exec_cmd(cmd_path, argv);
  } else {
    printf("%s: command not found\n", argv[0]);
  }
  for (char **args = argv; *args != NULL; args++) {
    free(*args);
  }
  free(argv);
}

void command_pwd() {
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));
  printf("%s\n", cwd);
}

void clean_input(char *input, int buffer_size) {
  if (fgets(input, buffer_size, stdin) != NULL) {
    size_t len = strlen(input);
    if (len > 0 && input[len - 1] == '\n') {
      input[len - 1] = '\0';
    }
  }
}

int main() {
  char input[BUFFER_SIZE];
  while (1) {
    printf("$ ");
    fflush(stdout);
    clean_input(input, sizeof(input));
    if (strncmp(input, "exit", sizeof("exit") - 1) == 0)
      command_exit(input + sizeof("exit"));
    else if (strncmp(input, "echo", sizeof("echo") - 1) == 0)
      command_echo(input + sizeof("echo"));
    else if (strncmp(input, "cd", sizeof("cd") - 1) == 0)
      command_cd(input + sizeof("cd"));
    else if (strncmp(input, "type", sizeof("type") - 1) == 0)
      command_type(input + sizeof("type"));
    else if (strncmp(input, "pwd", sizeof("pwd") - 1) == 0)
      command_pwd();
    else {
      command_execute(input);
    }
    fflush(stdout);
  }
  return 0;
}
