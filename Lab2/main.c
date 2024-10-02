#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>

#define MAX_CMD_LEN 1024
#define MAX_ARGS 64

#define DEBUG 0

void run_command(char *cmd);

void handle_redirection(char **args, int *in_fd, int *out_fd);

void handle_pipe(char **args1, char **args2);

void set_text_color(const char *color) {
    printf("%s", color);
}

void reset_text_color() {
    printf("\033[0m");
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

void run_command(char *cmd) {
    char *args[MAX_ARGS];
    char *args1[MAX_ARGS], *args2[MAX_ARGS];
    char *token;
    int in_f = -1, out_f = -1;
    int pipe_index = -1;
    int arg_count = 0;

    token = strtok(cmd, " \t");
    while (token != NULL && arg_count < MAX_ARGS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " \t");
    }
    args[arg_count] = NULL;

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

void handle_redirection(char **args, int *in_fd, int *out_fd) {
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