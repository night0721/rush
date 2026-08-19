#define _XOPEN_SOURCE 600

#include <dirent.h>
#include <termios.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include <time.h>
#include <stdbool.h>
#include <signal.h>
#include <ctype.h>
#include <sys/ioctl.h>

#include "constants.h"
#include "history.h"
#include "commands.h"
#include "alias.h"
#include "git.h"

void *memalloc(size_t size)
{
	void *ptr = malloc(size);
	if (!ptr) {
		fputs("90s: Error allocating memory\n", stderr);
		exit(EXIT_FAILURE);
	}
	return ptr;
}

int get_terminal_width(void) {
	struct winsize ws;
	if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0)
		return 80; // fallback
	return ws.ws_col;
}

void change_terminal_attribute(int option)
{  
	static struct termios oldt, newt;
	if (option) {
		tcgetattr(STDIN_FILENO, &oldt);
		newt = oldt;
		newt.c_lflag &= ~(ICANON | ECHO); // allows getchar without pressing enter key and echoing the character twice
		tcsetattr(STDIN_FILENO, TCSANOW, &newt); // set settings to stdin
	} else {
		tcsetattr(STDIN_FILENO, TCSANOW, &oldt); // restore to old settings
	}
}  

char **setup_path_variable(void)
{
	char *envpath = getenv("PATH");
	if (envpath == NULL) {
		fprintf(stderr, "90s: PATH environment variable is missing\n");
		exit(EXIT_FAILURE);
	}
	char *path_cpy = memalloc(strlen(envpath) + 1);
	char *path = memalloc(strlen(envpath) + 1);
	strcpy(path_cpy, envpath);
	strcpy(path, envpath);
	int path_count = 0;
	while (*path_cpy != '\0') {
		// count number of : to count number of elements
		if (*path_cpy == ':') {
			path_count++;
		}
		path_cpy++;
	}
	path_count += 2; // adding one to be correct and one for terminator
	char **paths = memalloc(sizeof(char *) * path_count);
	char *token = strtok(path, ":");
	int counter = 0;
	while (token != NULL) {
		paths[counter] = token; // set element to the pointer of start of path
		token = strtok(NULL, ":");
		counter++;
	}
	paths[counter] = NULL;
	return paths;
}

bool find_command(char **paths, char *command)
{
	if (strncmp(command, "", 1) == 0) {
		return false;
	}
	while (*paths != NULL) {
		char current_path[PATH_MAX];
		current_path[0] = '\0';
		sprintf(current_path, "%s/%s", *paths, command);
		if (access(current_path, X_OK) == 0) {
			// command is executable
			return true;
		} else {
			if (is_builtin(command)) {
				return true;
			}
		}
		paths++;
	}
	return false;
}

void shiftleft(int chars)
{
	printf("\033[%dD", chars);
}

void shiftright(int chars)
{
	printf("\033[%dC", chars);
}

void clearline(void)
{
	printf("\033[K"); // clear line to the right of cursor
}

int prompt_visible_length(const char *str)
{
	int len = 0;
	int in_esc = 0;

	if (!str)
		return 0;

	while (*str) {
		if (*str == '\033') {
			in_esc = 1;
		} else if (in_esc) {
			/* ESC sequences typically end at a letter (e.g., 'm' in \033[34m) */
			if ((*str >= 'A' && *str <= 'Z') || (*str >= 'a' && *str <= 'z'))
				in_esc = 0;
		} else {
			len++;
		}
		str++;
	}

	return len;
}

void highlight(char *buffer, char **paths)
{
	char *cmd_part = strchr(buffer, ' ');
	size_t cmd_len = cmd_part ? (size_t)(cmd_part - buffer) : strlen(buffer);

	char cmd[cmd_len + 1];
	memcpy(cmd, buffer, cmd_len);
	cmd[cmd_len] = '\0';
	bool valid = find_command(paths, cmd);

	/* green if valid, red if invalid */
	const char *cmd_color = valid ? "\x1b[32m" : "\x1b[31m";

	/* Print the colored command, then switch to white for args */
	printf("%s%s\x1b[37m", cmd_color, cmd);

	/* Walk the rest token by token, underline each token that has existing path */
	char *p = buffer + cmd_len;
	while (*p) {
		if (*p == ' ' || *p == '\t') {
			putchar(*p);
			p++;
			continue;
		}

		/* Find end of token (next whitespace) */
		char *start = p;
		while (*p && *p != ' ' && *p != '\t')
			p++;
		int tok_len = (int)(p - start);

		bool exists = false;
		if (tok_len > 0 && tok_len < PATH_MAX) {
			char tok[PATH_MAX];
			memcpy(tok, start, tok_len);
			tok[tok_len] = '\0';

			const char *check_path = tok;
			char expanded[PATH_MAX];

			if (tok[0] == '~') {
				char *home = getenv("HOME");
				if (home) {
					snprintf(expanded, sizeof(expanded), "%s%s",
							home, tok + 1);
					check_path = expanded;
				}
			}

			if (access(check_path, F_OK) == 0)
				exists = true;
		}

		if (exists)
			printf("\033[1m\033[4m%.*s\033[22m\033[24m", tok_len, start);
		else
			printf("%.*s", tok_len, start);
	}

	printf("\x1b[m");
	fflush(stdout);
}

void render(const char *prompt, const char *buffer, int cursor_pos,
		int *prev_lines_out, char **paths) {
	int width = get_terminal_width();
	// strip ANSI for counting
	int prompt_visible = prompt_visible_length(prompt);

	// Move cursor up to the first line if we were multiline
	if (*prev_lines_out > 1) {
		printf("\033[%dA", *prev_lines_out - 1);  // move up
	}
	printf("\r");  // go to start of line

	// Clear all lines we might have used
	for (int i = 0; i < *prev_lines_out; i++) {
		printf("\033[2K");  // clear entire line
		if (i < *prev_lines_out - 1) printf("\n");
	}

	// Move back up to first line
	if (*prev_lines_out > 1) {
		printf("\033[%dA", *prev_lines_out - 1);
	}

	printf("\r");

	// Print prompt + highlighted buffer
	printf("%s", prompt);
	highlight(buffer, paths);  // your existing highlight function

	// Calculate where cursor should be
	int total_visible = prompt_visible + cursor_pos;
	int target_line = total_visible / width;
	int target_col = total_visible % width;

	// Move cursor to correct position
	printf("\r");
	if (target_line > 0) {
		printf("\033[%dB", target_line);  // move down
	}
	if (target_col > 0) {
		printf("\033[%dC", target_col);   // move right
	}

	fflush(stdout);

	// Update prev_lines for next render
	int new_total = prompt_visible + strlen(buffer);
	*prev_lines_out = new_total / width + (new_total % width ? 1 : 0);
	if (*prev_lines_out < 1) *prev_lines_out = 1;
}

char *readline(char **paths, const char *prompt)
{
	int bufsize = RL_BUFSIZE;
	int position = 0;
	char *buffer = memalloc(bufsize);
	buffer[0] = '\0';

	int prev_lines = 1;
	int prompt_len = prompt_visible_length(prompt);

	printf("%s", prompt);
	fflush(stdout);

	buffer[0] = '\0';
	while (1) {
		// Read a character
		int c = getchar();
		int buf_len = strlen(buffer);

		switch (c) {
			case EOF:
				exit(EXIT_SUCCESS);

				// Enter
			case '\n':
				buffer[buf_len] = '\0';
				printf("\n");

				// Check if command includes !!
				if (strstr(buffer, "!!") != NULL) {
					char *last_command = read_command(1);
					if (last_command != NULL) {
						// replace !! with the last command
						char *pos = strstr(buffer, "!!");
						char tmp[1024];
						snprintf(tmp, sizeof(tmp), "%.*s%s%s", (int)(pos - buffer), buffer,
								last_command, pos + 2);
						/* Copy back or realloc to fit */
						buffer = realloc(buffer, strlen(tmp) + 1);
						strcpy(buffer, tmp);
						/* 						TODO: position += last_command_len - replace_len; */
						break;
					}
				}
				return buffer;

			case 127: // backspace
				if (buf_len > 0) {
					memmove(&buffer[position - 1], &buffer[position], buf_len - position + 1);
					position--;
				}
				break;

			case 27: // Arrow keys has three characters, 27, 91, then 65-68
				if (getchar() == '[') {
					int arrow = getchar();
					if (arrow == 'A') { // Up - history
						char *hist = read_command(1);
						if (hist) {
							strncpy(buffer, hist, bufsize-1);
							buffer[bufsize-1] = '\0';
							position = strlen(buffer);
						}
					} else if (arrow == 'B') { // Down
						char *hist = read_command(0);
						if (hist) {
							strncpy(buffer, hist, bufsize-1);
							buffer[bufsize - 1] = '\0';
							position = strlen(buffer);
						} else {
							buffer[0] = '\0';
							position = 0;
						}
					} else if (arrow == 'C') { // Right
						if (position < buf_len) position++;
					} else if (arrow == 'D') { // Left
						if (position > 0) position--;
					}
				}
				break;

			default:
				if (c > 31 && c < 127) {
					if (position == buf_len) {
						// Append character to the end of the buffer
						buffer[position] = c;
						buffer[position + 1] = '\0';
					} else {
						// Insert character at the current position
						memmove(&buffer[position+1], &buffer[position], buf_len - position + 1);
						buffer[position] = c;
					}
					position++;
				}
		}

		render(prompt, buffer, position, &prev_lines, paths);

		// If we have exceeded the buffer, reallocate.
		if ((strlen(buffer) + 1) >= bufsize) {
			bufsize += RL_BUFSIZE;
			buffer = realloc(buffer, bufsize);
			if (!buffer) {
				fprintf(stderr, "90s: Error allocating memory\n");
				exit(EXIT_FAILURE);
			}
		}
	}
}

void free_args(char **args)
{
	if (!args) return;
	for (int i = 0; args[i] != NULL; i++) {
		free(args[i]);
	}
	free(args);
}

// split line into arguments
char **argsplit(char *line)
{
	int bufsize = TOK_BUFSIZE, position = 0;
	char **tokens = memalloc(sizeof(char *) * bufsize);
	char *p = line;

	while (*p) {
		// skip leading whitespace
		while (*p && isspace((unsigned char)*p)) p++;
		if (!*p) break;

		int tok_cap = strlen(p) + 128; // 128 for ~
		char *token = memalloc(tok_cap);
		int tok_pos = 0;
		bool in_single = false;
		bool in_double = false;

		while (*p) {
			// Escape sequences
			if (!in_single && *p == '\\') {
				p++;
				if (*p) token[tok_pos++] = *p++;
				continue;
			}
			// Single quote
			if (!in_double && *p == '\'') {
				in_single = !in_single;
				p++;
				continue;
			}
			// Double quote
			if (!in_single && *p == '"') {
				in_double = !in_double;
				p++;
				continue;
			}

			if (*p == '~' && !in_single && !in_double) {
				// Make sure prev token is space
				if (tok_pos == 0 || isspace((unsigned char)token[tok_pos-1])) {
					char *home = getenv("HOME");
					if (home) {
						int home_len = strlen(home);
						if (tok_pos + home_len >= tok_cap) {
							tok_cap = tok_pos + home_len + 1;
							token = realloc(token, tok_cap);
						}
						memcpy(token + tok_pos, home, home_len);
						tok_pos += home_len;
						p++;
						continue;
					}
				}
			}

			// Create token if not in quote
			if (!in_single && !in_double && isspace((unsigned char)*p)) {
				break;
			}
			token[tok_pos++] = *p++;
		}
		token[tok_pos] = '\0';

		tokens[position] = token;
		position++;

		if (position >= bufsize) {
			bufsize += TOK_BUFSIZE;
			tokens = realloc(tokens, sizeof(char *) * bufsize);
			if (!tokens) {
				fprintf(stderr, "90s: Error allocating memory\n");
				exit(EXIT_FAILURE);
			}
		}
	}
	tokens[position] = NULL;
	return tokens;
}

char **modifyargs(char **args)
{
	int num_arg = 0;

	// check if command is ls, diff, or grep, if so, add --color=auto to the arguments
	// so they have color without user typing it
	while (args[num_arg] != NULL) {
		num_arg++;
	}
	for (int i = 0; i < num_arg; i++) {
		// makes ls and diff and grep have color without user typing it
		if (strncmp(args[i], "ls", 2) == 0 || strncmp(args[i], "diff", 4) == 0 || strncmp(args[i], "grep", 4) == 0) {
			for (int j = num_arg; j > i; j--) {
				args[j + 1] = args[j];
			}
			args[i + 1] = strdup("--color=auto");
			num_arg++;
		}
	}

	return args;
}

char *trimws(char *str)
{
	char *end;
	while (isspace((unsigned char) *str))
		str++;
	if(*str == 0)
		return str;
	end = str + strlen(str) - 1;
	while (end > str && isspace((unsigned char) *end))
		end--;
	*(end+1) = 0;
	return str;
}

char ***pipe_argsplit(char *line)
{
	char ***cmdv = memalloc(sizeof(char **) * 128); // 127 commands, 1 for NULL
	char **cmds = memalloc(sizeof(char *) * 128); // 127 arguments, 1 for NULL
	int num_arg = 0;
	char *pipe = strtok(line, "|");
	while (pipe != NULL) {
		pipe = trimws(pipe);
		cmds[num_arg] = expand_segment(pipe);
		pipe = strtok(NULL, "|");
		num_arg++;
	}
	cmds[num_arg] = NULL;

	for (int i = 0; i < num_arg; i++) {
		char **splitted = argsplit(cmds[i]);
		cmdv[i] = modifyargs(splitted);
	}
	cmdv[num_arg] = NULL;
	for (int i = 0; i < num_arg; i++) free(cmds[i]);
	free(cmds);
	return cmdv;
}

// continously prompt for command and execute it
void command_loop(char **paths)
{
	char *line;
	char **args;
	int status = 1;

	while (status) {
		/* Get current time */
		time_t t = time(NULL);
		struct tm *current_time = localtime(&t);
		char timestr[256];
		/* Format time string */
		if (strftime(timestr, sizeof(timestr), "[%H:%M:%S]", current_time) == 0) {
			return;
		}
		char cwd[PATH_MAX];
		/* Get current working directory */
		if (getcwd(cwd, PATH_MAX) == NULL) {
			return;
		}
		char *home = getenv("HOME");
		size_t home_len = strlen(home);

		int i = 0, j = 0;
		/* Check if cwd starts with home */
		if (home && strncmp(cwd, home, home_len) == 0) {
			cwd[j++] = '~';
			i += home_len;
		}
		while (cwd[i] != '\0') {
			cwd[j++] = cwd[i++];
		}
		cwd[j] = '\0';

		/* Blue time string, pink time, teal arrow */
		char *git_info = get_git_info();
		char git_str[128];
		if (git_info != NULL) {
			snprintf(git_str, sizeof(git_str), "\033[31m[%s]", git_info);
			free(git_info);
		}
		char prompt[256];
		snprintf(prompt, sizeof(prompt), "\033[34m%s \033[35m[%s] %s \033[36m> ", timestr, cwd, git_str);

		cmd_count = 0; // upward arrow key resets command count
		line = readline(paths, prompt);
		if (line == NULL) {
			printf("\n");
			continue;
		}
		save_command_history(line);

		char *expanded = expand_segment(line);
		free(line);
		line = expanded;

		bool has_pipe = false;
		bool in_single = false, in_double = false;
		for (int i = 0; line[i] != '\0'; i++) {
			if (line[i] == '\'' && !in_double) in_single = !in_single;
			else if (line[i] == '"' && !in_single) in_double = !in_double;
			else if (line[i] == '|' && !in_single && !in_double) {
				has_pipe = true;
				break;
			}
		}

		if (has_pipe) {
			char ***pipe_args = pipe_argsplit(line);
			status = execute_pipe(pipe_args);

			char ***tmp = pipe_args;
			while (*tmp != NULL) {
				free_args(*tmp);
				tmp++;
			}
			free(pipe_args);
		} else {
			args = argsplit(line);
			args = modifyargs(args);
			status = execute(args, STDOUT_FILENO, OPT_FGJ);
			free_args(args);
		}
		free(line);
	};
}

void quit_sig(int sig)
{
	exit(EXIT_SUCCESS);
}

void cleanup_terminal(void)
{
	change_terminal_attribute(0);
	printf("\033[?25h"); // show cursor
	fflush(stdout);
}

int main(int argc, char **argv)
{
	// setup
	atexit(cleanup_terminal);
	signal(SIGINT, quit_sig);
	signal(SIGTERM, quit_sig);
	signal(SIGQUIT, quit_sig);
	check_history_file();
	check_aliases_file();
	char **paths = setup_path_variable();
	change_terminal_attribute(1); // turn off echoing and disabling getchar requires pressing enter key to return

	command_loop(paths);

	// cleanup
	free(paths);
	change_terminal_attribute(0); // change back to default settings
	return EXIT_SUCCESS;
}
