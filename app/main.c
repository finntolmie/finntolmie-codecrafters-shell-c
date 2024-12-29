#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <fcntl.h>
#include <libgen.h>
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
#define MAX_ARG_COUNT 24

char **argv;

struct redirection {
  int out_fd;
  int err_fd;
};

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

  size_t buf_size = MAX_ARG_COUNT;
  size_t argc = 0;
  char **argv = malloc(buf_size * sizeof(char *));
  if (argv == NULL) {
    perror("malloc");
    return NULL;
  }

  const char *ptr = args;
  char quote_char;
  while (*ptr != '\0' && argc < buf_size) {
    while (isspace((unsigned char)*ptr))
      ptr++;
    if (*ptr == '\0')
      break;

    const char *start = ptr;
    char *arg = malloc(1024);
    if (arg == NULL) {
      perror("malloc");
      goto parse_error;
    }
    size_t arglen = 0;
  restart:
    quote_char = '\0';
    if (*ptr == '\'' || *ptr == '"') {
      quote_char = *ptr;
      ptr++;
    }

    while (*ptr != '\0' && ((quote_char && *ptr != quote_char) ||
                            (!quote_char && !isspace((unsigned char)*ptr)))) {
      if (*ptr == '\\' && !quote_char) {
        ptr++;
        if (*ptr == '\0')
          break;
      }
      if (*ptr == '\\' && quote_char == '"') {
        char peek = ptr[1];
        if (peek == '\\' || peek == '$' || peek == '"') {
          ptr++;
          if (*ptr == '\0')
            break;
        }
      }
      arg[arglen++] = *ptr++;
    }

    if (quote_char && *ptr == quote_char) {
      ptr++;
      // disgusting hack
      if (!isspace((unsigned char)*ptr))
        goto restart;
    }

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
  for (char **args = argv; *args != NULL; args++)
    free(*args);
  free(argv);
  return NULL;
}

char **parse_stdout_redirects(char **argv) {
  char **redirects = malloc((MAX_ARG_COUNT / 2) * sizeof(char *));
  if (redirects == NULL) {
    perror("malloc");
    return NULL;
  }
  int i = 0;
  int arglen = 0;
  while (argv[i] != NULL) {
    if (strcmp(argv[i++], "1>") != 0 || strcmp(argv[i++], ">") != 0)
      continue;
    if (argv[i] == NULL)
      break;

    redirects[arglen++] = strdup(argv[i]);
    if (redirects[arglen - 1] == NULL) {
      perror("strdup");
      for (int j = 0; j < arglen; j++)
        free(redirects[j]);
      free(redirects);
      return NULL;
    }
    free(argv[i - 1]);
    free(argv[i]);
    int j = i - 1;
    while (argv[j + 2] != NULL) {
      argv[j] = argv[j + 2];
      j++;
    }
    argv[j] = NULL;
    argv[j + 1] = NULL;
  }
  redirects[arglen] = NULL;
  return redirects;
}

void exec_cmd(char **argv) {
  pid_t pid, wpid;
  int status;
  if (argv[0] == NULL)
    return;
  if (find_in_path(argv[0]) == NULL) {
    fprintf(stderr, "%s: command not found\n", argv[0]);
    return;
  }
  pid = fork();
  if (pid == 0) {
    if (execvp(argv[0], argv) == -1) {
      perror("launch");
    }
    exit(EXIT_FAILURE);
  } else if (pid < 0) {
    perror("launch");
  } else {
    do {
      wpid = waitpid(pid, &status, WUNTRACED);
    } while (!WIFEXITED(status) && !WIFSIGNALED(status));
  }
}

void command_echo(char **argv) {
  char **argstdout;
  int first = 1;
  // skip echo
  argv++;
  for (char **args = argv; *args != NULL; args++) {
    if (!first)
      printf(" ");
    printf("%s", *args);
    first = 0;
  }
  printf("\n");
}

char *replaceWord(const char *s, const char *oldW, const char *newW) {
  char *result;
  int i, cnt = 0;
  int newWlen = strlen(newW);
  int oldWlen = strlen(oldW);
  for (i = 0; s[i] != '\0'; i++) {
    if (strstr(&s[i], oldW) == &s[i]) {
      cnt++;
      i += oldWlen - 1;
    }
  }
  result = (char *)malloc(i + cnt * (newWlen - oldWlen) + 1);
  i = 0;
  while (*s) {
    if (strstr(s, oldW) == s) {
      strcpy(&result[i], newW);
      i += newWlen;
      s += oldWlen;
    } else
      result[i++] = *s++;
  }
  result[i] = '\0';
  return result;
}

void command_cd(char **argv) {
  if (argv[1] == NULL) {
    fprintf(stderr, "Error: expected argument to cd\n");
    return;
  }
  if (strncmp(argv[1], "~", 2) == 0) {
    char *home = getenv("HOME");
    if (home == NULL) {
      fprintf(stderr, "Error: $HOME is not set\n");
    }
    char *path = replaceWord("~", argv[1], home);
    chdir(path);
  } else if (chdir(argv[1]) != 0) {
    fprintf(stderr, "cd: %s: No such file or directory\n", argv[1]);
  }
}

void command_type(char **argv) {
  if (argv[1] == NULL) {
    fprintf(stderr, "Error: expected argument\n");
    return;
  }
  for (const char **cmd = builtins; *cmd != NULL; cmd++) {
    if (strcmp(*cmd, argv[1]) == 0) {
      printf("%s is a shell builtin\n", argv[1]);
      return;
    }
  }
  char *path = find_in_path(argv[1]);
  if (path) {
    printf("%s is %s\n", argv[1], path);
  } else {
    printf("%s: not found\n", argv[1]);
  }
}

void command_execute(char **argv) {
  char *cmd_path = find_in_path(argv[0]);
  if (cmd_path) {
    exec_cmd(argv);
  } else {
    fprintf(stderr, "%s: command not found\n", argv[0]);
  }
}

void command_pwd() {
  char cwd[1024];
  getcwd(cwd, sizeof(cwd));
  printf("%s\n", cwd);
}

char **handle_redirection(char **args, struct redirection *redir) {
  char **new_args = malloc(sizeof(char *) * BUFSIZ);
  if (args[0] == NULL) {
    return NULL;
  }
  int i = 0, j = 0;
  while (args[i] != NULL) {
    if (strcmp(args[i], ">") == 0 || strcmp(args[i], "1>") == 0 ||
        strcmp(args[i], ">>") == 0 || strcmp(args[i], "1>>") == 0) {
      if (args[i + 1] == NULL) {
        fprintf(stderr, "Error: expected filename/stream after >\n");
        return NULL;
      }
      int flags = O_WRONLY | O_CREAT;
      flags |= (strcmp(args[i], ">>") == 0 || strcmp(args[i], "1>>") == 0)
                   ? O_APPEND
                   : O_TRUNC;
      redir->out_fd = open(args[i + 1], flags, 0644);
      if (redir->out_fd == -1) {
        perror("open");
        return NULL;
      }
      i += 2; // advance past the redirection
      continue;
    }
    if (strcmp(args[i], "2>") == 0 || strcmp(args[i], "2>>") == 0) {
      if (args[i + 1] == NULL) {
        fprintf(stderr, "Error: expected filename/stream after >\n");
        return NULL;
      }
      int flags = O_WRONLY | O_CREAT;
      flags |= (strcmp(args[i], "2>>") == 0) ? O_APPEND : O_TRUNC;
      redir->err_fd = open(args[i + 1], flags, 0644);
      if (redir->err_fd == -1) {
        perror("open");
        return NULL;
      }
      i += 2; // advance past the redirection
      continue;
    }
    new_args[j++] = strdup(args[i++]);
  }
  new_args[j] = NULL;
  return new_args;
}

void restore_redirection(struct redirection *red, int saved_stdout,
                         int saved_stderr) {
  if (red->out_fd != -1) {
    dup2(saved_stdout, STDOUT_FILENO);
    close(red->out_fd);
    close(saved_stdout);
  }
  if (red->err_fd != -1) {
    dup2(saved_stderr, STDOUT_FILENO);
    close(red->err_fd);
    close(saved_stderr);
  }
}

int has_redirection(char **args) {
  for (int i = 0; args[i] != NULL; i++) {
    if (strstr(args[i], ">") != NULL)
      return 1;
  }
  return 0;
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
  int exit_no = -1;
  while (exit_no == -1) {
    struct redirection redir = {-1, -1};
    int saved_stdout = -1;
    int saved_stderr = -1;
    printf("$ ");
    fflush(stdout);
    clean_input(input, sizeof(input));
    argv = parse_args(input);
    if (argv[0] == NULL) {
      fflush(stdout);
      continue;
    }
    char **cmd_args = argv;
    if (has_redirection(argv)) {
      cmd_args = handle_redirection(argv, &redir);
      if (cmd_args == NULL) {
        fflush(stdout);
        continue;
      }
      if (redir.out_fd != -1) {
        saved_stdout = dup(STDOUT_FILENO);
        dup2(redir.out_fd, STDOUT_FILENO);
      }
      if (redir.err_fd != -1) {
        saved_stderr = dup(STDERR_FILENO);
        dup2(redir.err_fd, STDERR_FILENO);
      }
    }
    if (strcmp(cmd_args[0], "exit") == 0) {
      if (cmd_args[1] == NULL)
        exit_no = 0;
      else
        exit_no = atoi(cmd_args[1]);
    } else if (strcmp(cmd_args[0], "echo") == 0)
      command_echo(cmd_args);
    else if (strcmp(cmd_args[0], "cd") == 0)
      command_cd(cmd_args);
    else if (strcmp(cmd_args[0], "type") == 0)
      command_type(cmd_args);
    else if (strcmp(cmd_args[0], "pwd") == 0)
      command_pwd();
    else {
      command_execute(cmd_args);
    }
    restore_redirection(&redir, saved_stdout, saved_stderr);
    if (cmd_args != argv) {
      for (char **args = cmd_args; *args != NULL; args++)
        free(*args);
      free(cmd_args);
    }
    for (char **args = argv; *args != NULL; args++)
      free(*args);
    free(argv);
    fflush(stdout);
  }
  return 0;
}
