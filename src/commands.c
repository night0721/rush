#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/wait.h>

#include "constants.h"
#include "history.h"
#include "90s.h"
#include "job.h"

int execute(char **args);

/* Builtin commands */
int cd(char **args);
int help(char **args);
int quit(char **args);
int history(char **args);
int export(char **args);
int source(char **args);
int j(char **args);
int bg(char **args);

char *builtin_cmds[] = {
    "cd",
    "help",
    "exit",
    "history",
    "export",
    "source",
    "j",
    "bg",
};

int (*builtin_func[]) (char **) = {
    &cd,
    &help,
    &quit, /* Can't name it exit as it is taken */
    &history,
    &export,
    &source,
    &j,
    &bg,
};

char *shortcut_dirs[] = {
    "90s",
    "bin",
    "localbin",
};

char *shortcut_expand_dirs[] = {
    "~/.nky/git/90s",
    "~/.local/bin",
    "/usr/local/bin",
};

char *gethome(void)
{
    char *home = getenv("HOME");
    if (home == NULL) {
        fprintf(stderr, "Error: HOME environment variable not set.\n");
        exit(EXIT_FAILURE);
    }
    return home;
}

char *replace_home_dir(char *str)
{
    char *home_path = gethome();

    int path_len = strlen(str);
    int home_len = strlen(home_path);

    // Allocate memory for the new path
    char* new_path = memalloc(path_len + home_len + 1);

    int i = 0, j = 0;
    while (str[i] != '\0') {
        if (str[i] == '~') {
            // Copy HOME environment variable value
            for (int k = 0; k < home_len; k++) {
                new_path[j++] = home_path[k];
            }
            i++;
        } else {
            new_path[j++] = str[i++];
        }
    }

    new_path[j] = '\0';
    return new_path;
}

// number of built in commands
int num_builtins(void) {
    return sizeof(builtin_cmds) / sizeof(char *);
}

// autojump
int j(char **args)
{
    if (args[1] == NULL) {
        fprintf(stderr, "90s: not enough arguments\n");
        return -1;
    }
    for (size_t i = 0; i < sizeof(shortcut_dirs) / sizeof(char *); i++) {
        int len = strlen(shortcut_dirs[i]);
        if (strncmp(args[1], shortcut_dirs[i], len) == 0) {
            char **merged_cd = memalloc(sizeof(char *) * 3);
            merged_cd[0] = "cd";
            merged_cd[1] = shortcut_expand_dirs[i];
            merged_cd[2] = NULL;
            cd(merged_cd);
            printf("jumped to %s\n", shortcut_expand_dirs[i]);
            return 1;
        }
    }
    return 1;
}

/*
 * Change directory
 */
int cd(char **args) {
    int i = 0;
    if (args[1] == NULL) {
        char *home = gethome();
        if (chdir(home) != 0) {
            perror("90s");
        }
    } else {
        while (args[1][i] != '\0') {
            if (args[1][i] == '~') {
                args[1] = replace_home_dir(args[1]);
                break;
            }
            i++;
        }
        if (chdir(args[1]) != 0) {
            perror("90s");
        }
    }
    return 1;
}

/*
 * Show help menu
 */
int help(char **args)
{
	(void) args;
    printf("90s %f\n", VERSION);
    printf("Built in commands:\n");

    for (int i = 0; i < num_builtins(); i++) {
        printf("  %s\n", builtin_cmds[i]);
    }

    printf("Use 'man' to read manual of programs\n");
    printf("Licensed under GPL v3\n");
    return 1;
}

int quit(char **args)
{
	(void) args;
	/* Exit prompt loop */
    return 0;
}

int history(char **args)
{
	(void) args;
    char **history = get_all_history(true);

    for (int i = 0; history[i] != NULL; ++i) {
        printf("%s\n", history[i]);
        free(history[i]);
    }

    free(history);
    return 1;
}

int export(char **args)
{
	/* Skip the command */
    args++;
    while (*args != NULL) {
        char *variable = strtok(*args, "=\n");
        char *value = strtok(NULL, "=\n");
        if (variable != NULL && value != NULL) {
            if (setenv(variable, value, 1) != 0) {
                fprintf(stderr, "90s: Error setting environment variable\n");
                return 0;
            }
        } else {
            fprintf(stderr, "90s: Syntax error when setting environment variable\nUse \"export VARIABLE=VALUE\"\n");
            return 0;
        }
        args++;
    }
    return 1;
}

int source(char **args)
{
    if (args[1] == NULL) {
        fprintf(stderr, "90s: not enough arguments\n");
        return -1;
    }

    FILE *file = fopen(args[1], "r");

    if (file == NULL) {
        fprintf(stderr, "90s: no such file or directory '%s'\n", args[1]);
        return -1;
    }

    char line[RL_BUFSIZE];
    int status;
    while (fgets(line, sizeof(line), file) != NULL) {
        // Remove newline character if present
        size_t len = strlen(line);
        if (len > 0 && line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        char **args = argsplit(line);
        status = execute(args);
    }

    fclose(file);
    return status; // Indicate success
}

int bg(char **args)
{
    if (args[1] == NULL) {
        fprintf(stderr, "90s: not enough arguments\n");
        return -1;
    }
    int job_index = atoi(args[1]);
    if (job_index == 0) {
        fprintf(stderr, "90s: invalid job index\n");
        return -1;
    }
    job *search = get_job(job_index - 1);
    if (search == NULL) {
        fprintf(stderr, "90s: no such job\n");
        return -1;
    }
    printf("Job %i: %s\n", job_index, search->command);
    return 1;
}

bool is_builtin(char *command)
{
    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(command, builtin_cmds[i]) == 0) {
            return true;
        }
    }
    return false;
}

int launch(char **args, int fd, int options)
{
    int is_bgj = (options & OPT_BGJ) ? 1 : 0;
    int redirect_stdout = (options & OPT_STDOUT) ? 1 : 0;
    int redirect_stdin = (options & OPT_STDIN) ? 1 : 0;
    int redirect_stderr = (options & OPT_STDERR) ? 1 : 0;

    pid_t pid;

    int status;
    if ((pid = fork()) == 0) {
        // Child process
        if (fd > 2) {
            // not stdin, stdout, or stderr
            if (redirect_stdout) {
                if (dup2(fd, STDOUT_FILENO) == -1) {
                    perror("90s");
                }
            }
            if (redirect_stdin) {
                if (dup2(fd, STDIN_FILENO) == -1) {
                    perror("90s");
                }
            }
            if (redirect_stderr) {
                if (dup2(fd, STDERR_FILENO) == -1) {
                    perror("90s");
                }
            }
            close(fd); // close fd as it is duplicated already
        }
        if (execvp(args[0], args) == -1) {
            if (errno == ENOENT) {
                fprintf(stderr, "90s: command not found: %s\n", args[0]);
            }
        }
        exit(EXIT_FAILURE); // exit the child
    } else if (pid < 0) {
        perror("fork failed");
    } else {
        // Parent process
        if (is_bgj) {
            int job_index = add_job(pid, args[0], true);
            printf("[Job: %i] [Process ID: %i] [Command: %s]\n", job_index + 1, pid, args[0]);
            return 1;
        } else {
            do {
                waitpid(pid, &status, WUNTRACED); // wait child to be exited to return to prompt
            } while (!WIFEXITED(status) && !WIFSIGNALED(status));
        }
    }
    return 1;
}
// execute built in commands or launch commands and wait it to terminate, return 1 to keep shell running
int execute(char **args)
{
    if (args[0] == NULL) { // An empty command was entered.
        return 1;
    }
    
    // prioritize builtin commands
    for (int i = 0; i < num_builtins(); i++) {
        if (strcmp(args[0], builtin_cmds[i]) == 0) {
            return (*builtin_func[i])(args);
        }
    }
    int num_arg = 0;

    while (args[num_arg] != NULL) {
        if (strncmp(args[num_arg], "&", 1) == 0) {
            args[num_arg] = NULL;
            if (args[num_arg + 1] != NULL) {
                // have commands after &
                execute(&args[num_arg + 1]);
            }
            launch(args, STDOUT_FILENO, OPT_BGJ);
            return 1;
        }
        if (strncmp(args[num_arg], ">", 1) == 0) {
            int fd = fileno(fopen(args[num_arg + 1], "w+"));
            if (fd == -1) {
                perror("90s");
                return 1;
            }
            args[num_arg] = NULL;
            int asdf = 0;
            while (args[asdf] != NULL) {
                fprintf(stderr, "args[%i]: %s\n", asdf, args[asdf]);
                asdf++;
            }

            return launch(args, fd, OPT_FGJ | OPT_STDOUT);
        }
        if (strncmp(args[num_arg], "<", 1) == 0) {
            int fd = fileno(fopen(args[num_arg + 1], "r"));
            if (fd == -1) {
                perror("90s");
                return 1;
            }
            args[num_arg] = NULL;
            return launch(args, fd, OPT_FGJ | OPT_STDIN);
        }
        if (strncmp(args[num_arg], "2>", 2) == 0) {
            int fd = fileno(fopen(args[num_arg + 1], "w+"));
            if (fd == -1) {
                perror("90s");
                return 1;
            }
            args[num_arg] = NULL;
            return launch(args, fd, OPT_FGJ | OPT_STDERR);
        }
        if (strncmp(args[num_arg], ">&", 2) == 0) {
            int fd = fileno(fopen(args[num_arg + 1], "w+"));
            if (fd == -1) {
                perror("90s");
                return 1;
            }
            args[num_arg] = NULL;
            return launch(args, fd, OPT_FGJ | OPT_STDOUT | OPT_STDERR);
        }
        if (strncmp(args[num_arg], ">>", 2) == 0) {
            int fd = fileno(fopen(args[num_arg + 1], "a+"));
            if (fd == -1) {
                perror("90s");
                return 1;
            }
            args[num_arg] = NULL;
            return launch(args, fd, OPT_FGJ | OPT_STDOUT);
        }

        num_arg++; // count number of args
    }

    return launch(args, STDOUT_FILENO, OPT_FGJ);
}

// execute_pipe with as many pipes as needed
int execute_pipe(char ***args)
{
    int pipefd[2];
    pid_t pid;
    int in = 0;

    int num_cmds = 0;
    while (args[num_cmds] != NULL) {
        int num_args = 0;
        while (args[num_cmds][num_args] != NULL) {
            num_args++;
        }
        num_cmds++;
    }
    
    for (int i = 0; i < num_cmds - 1; i++) {
        pipe(pipefd);
        if ((pid = fork()) == 0) {
            // then this (child)
            dup2(in, STDIN_FILENO); // get input from previous command
            if (i < num_cmds - 1) {
                dup2(pipefd[1], STDOUT_FILENO); // make output go to pipe (next output)
            }
            close(pipefd[0]); // close original input
            execute(args[i]);
            exit(EXIT_SUCCESS);
        } else if (pid < 0) {
            perror("fork failed");
        }
        // this will be executed first
        close(pipefd[1]); // close output
        in = pipefd[0]; // save the input for the next command
    }

    if ((pid = fork()) == 0) {
        dup2(in, STDIN_FILENO); // get input from pipe
        execute(args[num_cmds - 1]);
        exit(EXIT_SUCCESS);
    } else if (pid < 0) {
        perror("fork failed");
    }

    close(in);
    for (int i = 0; i < num_cmds; i++) {
        waitpid(pid, NULL, 0);
    }
    return 1;
}
