#ifndef COMMANDS_H_
#define COMMANDS_H_

#include <stdbool.h>

int num_builtins(void);
bool is_builtin(char *command);
int launch(char **args, int fd, int options);
int execute(char **args);
int execute_pipe(char ***args);

extern char *builtin_cmds[];

#endif
