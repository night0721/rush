#define _XOPEN_SOURCE 600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

typedef struct {
	char *name;
	char *value;
} alias_t;

static alias_t *aliases = NULL;
static int alias_count = 0;
static int alias_capacity = 0;

static char *trim_str(char *str)
{
	while (*str == ' ' || *str == '\t') str++;
	if (*str == '\0') return str;
	char *end = str + strlen(str) - 1;
	while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
		*end = '\0';
		end--;
	}
	return str;
}

void check_aliases_file(void)
{
	char *home = getenv("HOME");
	if (!home) return;

	char path[PATH_MAX];
	snprintf(path, sizeof(path), "%s/.rc", home);

	FILE *fp = fopen(path, "r");
	if (!fp) return;

	char line_buf[1024];
	while (fgets(line_buf, sizeof(line_buf), fp) != NULL) {
		char *line = trim_str(line_buf);
		// Skip empty line and comment
		if (*line == '\0' || *line == '#') continue;

		// Trim alias word away
		if (strncmp(line, "alias ", 6) == 0) {
			line += 6;
		}

		char *eq = strchr(line, '=');
		if (!eq) continue;

		*eq = '\0';
		char *name = trim_str(line);
		char *val_ptr = trim_str(eq + 1);

		// Remove quotes
		int val_len = strlen(val_ptr);
		if (val_len >= 2) {
			if ((val_ptr[0] == '"' && val_ptr[val_len - 1] == '"') ||
					(val_ptr[0] == '\'' && val_ptr[val_len - 1] == '\'')) {
				val_ptr[val_len - 1] = '\0';
				val_ptr++;
			}
		}

		if (alias_count >= alias_capacity) {
			alias_capacity += 10;
			aliases = realloc(aliases, sizeof(alias_t) * alias_capacity);
			if (!aliases) { perror("90s: realloc"); exit(EXIT_FAILURE); }
		}

		aliases[alias_count].name = strdup(name);
		aliases[alias_count].value = strdup(val_ptr);
		alias_count++;
	}
	fclose(fp);
}

// Expand command if possible, alwayas malloc a new string so need to free
char *expand_segment(const char *segment)
{
	const char *start = segment;
	while (*start == ' ' || *start == '\t') start++;
	const char *end = start;
	while (*end && *end != ' ' && *end != '\t') end++;

	int word_len = end - start;

	for (int i = 0; i < alias_count; i++) {
		if ((int)strlen(aliases[i].name) == word_len && strncmp(start, aliases[i].name, word_len) == 0) {
			// Swap
			char *new_seg = malloc(strlen(aliases[i].value) + strlen(end) + 1);
			if (!new_seg) { perror("90s: malloc"); exit(EXIT_FAILURE); }
			sprintf(new_seg, "%s%s", aliases[i].value, end);
			return new_seg;
		}
	}

	// no hit, return strdup
	return strdup(segment);
}
