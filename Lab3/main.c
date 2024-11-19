#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <fnmatch.h>
#include <sys/wait.h>
#include <ctype.h>
#include <unistd.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64
#define MAX_VARS 64
#define MAX_ALIASES 64

#define DEBUG 1

typedef struct {
    char *name;
    char *value;
} Variable;

typedef struct {
    char *name;
    char *command;
} Alias;

Variable variables[MAX_VARS];
Alias aliases[MAX_ALIASES];
int varCount = 0;
int aliasCount = 0;

void run_command(char *cmd);

void handle_redirection(char **args, const int *in_fd, const int *out_fd);

void handle_pipe(char **args1, char **args2);

void set_text_color(const char *color);

void reset_text_color();

void set_variable(char *name, char *value);

char *get_variable(char *name);

void list_variables();

void set_alias(char *name, char *command);

char *get_alias(char *name);

void list_aliases();

void expand_variables(char *cmd);

void expand_aliases(char **args);

void handle_glob_patterns(char **args);

void set_text_color(const char *color) {
    printf("%s", color);
}

void reset_text_color() {
    printf("\033[0m");
}

void set_variable(char *name, char *value) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            free(variables[i].value);
            variables[i].value = strdup(value);
            return;
        }
    }
    variables[varCount].name = strdup(name);
    if (value[0] == '\"' && value[strlen(value) - 1] == '\"') {
        value[strlen(value) - 1] = '\0';
        value++;
    }
    variables[varCount].value = strdup(value);
    varCount++;
}

char *get_variable(char *name) {
    for (int i = 0; i < varCount; i++) {
        if (strcmp(variables[i].name, name) == 0) {
            return variables[i].value;
        }
    }
    return NULL;
}

void list_variables() {
    for (int i = 0; i < varCount; i++) {
        printf("%s=%s\n", variables[i].name, variables[i].value);
    }
}

void set_alias(char *name, char *command) {
    char *arg_start = command;
    char *arg_end = command + strlen(command) - 1;
    if (*arg_start == '\"' && *arg_end == '\"') {
        arg_start++;
        *arg_end = '\0';
    }

    for (int i = 0; i < aliasCount; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            free(aliases[i].command);
            aliases[i].command = strdup(arg_start);
            return;
        }
    }
    aliases[aliasCount].name = strdup(name);
    aliases[aliasCount].command = strdup(arg_start);
    aliasCount++;
}

char *get_alias(char *name) {
    for (int i = 0; i < aliasCount; i++) {
        if (strcmp(aliases[i].name, name) == 0) {
            return aliases[i].command;
        }
    }
    return NULL;
}

void list_aliases() {
    for (int i = 0; i < aliasCount; i++) {
        printf("%s=%s\n", aliases[i].name, aliases[i].command);
    }
}

void expand_variables(char *cmd) {
    char buf[MAX_CMD_LEN];
    char *src = cmd, *dst = buf;
    while (*src) {
        if (*src == '$') {
            src++;
            char var_name[MAX_CMD_LEN];
            char *var_start = var_name;
            while (isalnum(*src) || *src == '_') {
                *var_start++ = *src++;
            }
            *var_start = '\0';
            char *value = get_variable(var_name);
            if (value) {
                while (*value) {
                    *dst++ = *value++;
                }
            }
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
    strcpy(cmd, buf);
}

void expand_aliases(char **args) {
    char *alias_command = get_alias(args[0]);
    if (alias_command) {
        char buffer[MAX_CMD_LEN];
        strcpy(buffer, alias_command);
        for (int i = 1; args[i] != NULL; i++) {
            strcat(buffer, " ");
            strcat(buffer, args[i]);
        }

        char *token = strtok(buffer, " ");
        int arg_count = 0;
        while (token != NULL && arg_count < MAX_ARGS - 1) {
            args[arg_count++] = token;
            token = strtok(NULL, " ");
        }
        args[arg_count] = NULL;
    }
}

void handle_glob_patterns(char **args) {
    char *new_args[MAX_ARGS];
    int new_arg_count = 0;

    for (int i = 0; args[i] != NULL; i++) {
        if (strchr(args[i], '*')) {
            DIR *dir = opendir(".");
            struct dirent *entry;
            while ((entry = readdir(dir)) != NULL) {
                if (entry->d_name[0] != '.' && fnmatch(args[i], entry->d_name, 0) == 0) {
                    new_args[new_arg_count++] = strdup(entry->d_name);
                }
            }
            closedir(dir);
        } else {
            new_args[new_arg_count++] = args[i];
        }
    }
    new_args[new_arg_count] = NULL;

    for (int i = 0; i < new_arg_count; i++) {
        args[i] = new_args[i];
    }
    args[new_arg_count] = NULL;
}

void handle_redirection(char **args, const int *in_fd, const int *out_fd) {
    pid_t pid = fork();
    if (pid == 0) {
        if (*in_fd != -1) {
            dup2(*in_fd, STDIN_FILENO);
            close(*in_fd);
        }
        if (*out_fd != -1) {
            dup2(*out_fd, STDOUT_FILENO);
            close(*out_fd);
        }
        execvp(args[0], args);
        perror("execvp");
        exit(EXIT_FAILURE);
    } else if (pid > 0) {
        wait(NULL);
    } else {
        perror("fork");
    }
}

void handle_pipe(char **args1, char **args2) {
    int pipe_fd[2];
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        return;
    }
    pid_t pid1 = fork();
    if (pid1 == 0) {
        close(pipe_fd[0]);
        dup2(pipe_fd[1], STDOUT_FILENO);
        close(pipe_fd[1]);
        execvp(args1[0], args1);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    pid_t pid2 = fork();
    if (pid2 == 0) {
        close(pipe_fd[1]);
        dup2(pipe_fd[0], STDIN_FILENO);
        close(pipe_fd[0]);
        execvp(args2[0], args2);
        perror("execvp");
        exit(EXIT_FAILURE);
    }

    close(pipe_fd[0]);
    close(pipe_fd[1]);
    wait(NULL);
    wait(NULL);
}

void run_command(char *cmd) {
    expand_variables(cmd);

    char *args[MAX_ARGS];
    char *args1[MAX_ARGS], *args2[MAX_ARGS];
    int in_f = -1, out_f = -1;
    int pipe_index = -1;
    int arg_count = 0;
    int in_quotes = 0;
    char *start = cmd;

    while (*cmd) {
        if (*cmd == '\"') {
            in_quotes = !in_quotes;
        } else if (!in_quotes && (*cmd == ' ' || *cmd == '\t')) {
            if (start != cmd) {
                *cmd = '\0';
                args[arg_count++] = start;
            }
            start = cmd + 1;
        }
        cmd++;
    }
    if (start != cmd) {
        args[arg_count++] = start;
    }
    args[arg_count] = NULL;

    if (strcmp(args[0], "set") == 0) {
        if (args[1] == NULL) {
            list_variables();
        } else if (args[2] == NULL) {
            char *value = get_variable(args[1]);
            if (value) {
                printf("%s\n", value);
            }
        } else {
            set_variable(args[1], args[2]);
        }
        return;
    }

    if (strcmp(args[0], "alias") == 0) {
        if (args[1] == NULL) {
            list_aliases();
        } else if (args[2] == NULL) {
            char *command = get_alias(args[1]);
            if (command) {
                printf("%s\n", command);
            }
        } else {
            set_alias(args[1], args[2]);
        }
        return;
    }

    expand_aliases(args);
    handle_glob_patterns(args);

    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "|") == 0) {
            pipe_index = i;
            break;
        }
    }

    if (pipe_index != -1) {
        args[pipe_index] = NULL;
        arg_count = 0;
        for (int i = 0; i < pipe_index; i++) {
            args1[arg_count++] = args[i];
        }
        args1[arg_count] = NULL;

        arg_count = 0;
        for (int i = pipe_index + 1; args[i] != NULL; i++) {
            args2[arg_count++] = args[i];
        }
        args2[arg_count] = NULL;

        handle_pipe(args1, args2);
        return;
    }

    arg_count = 0;
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            in_f = open(args[++i], O_RDONLY);
        } else if (strcmp(args[i], ">") == 0) {
            out_f = open(args[++i], O_WRONLY | O_CREAT | O_TRUNC, 0644);
        } else {
            args1[arg_count++] = args[i];
        }
    }
    args1[arg_count] = NULL;

    handle_redirection(args1, &in_f, &out_f);
}

int main() {
    if (DEBUG) {
        setvbuf(stdout, NULL, _IONBF, 0);
    }
    char input[MAX_CMD_LEN];
    while (1) {
        set_text_color("\033[1;32m");
        printf("GitBasher> ");
        reset_text_color();
        if (fgets(input, sizeof(input), stdin) == NULL) {
            break;
        }
        input[strcspn(input, "\n")] = '\0';
        if (strcmp(input, "exit") == 0) {
            break;
        }
        run_command(input);
    }
    return 0;
}