#ifndef _POSIX_C_SOURCE
#define _POSIX_C_SOURCE 2
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "90s.h"

// Trim end of string
void trim_newline(char *str)
{
    int len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[--len] = '\0';
    }
}

// Git status from malloc-ed string
char *get_git_info(void)
{
    FILE *fp = popen("git status -b --porcelain 2>/dev/null", "r");
    if (!fp) return NULL;

    char first_line[256];
    if (fgets(first_line, sizeof(first_line), fp) == NULL) {
		// Not git repo
        pclose(fp);
        return NULL;
    }
    trim_newline(first_line);

    // Check for 1+ lines (untracked/modified files)
    int is_dirty = 0;
    char second_line[256];
    if (fgets(second_line, sizeof(second_line), fp) != NULL) {
        is_dirty = 1;
    }
    pclose(fp);

    // Skip ##
    char *p = first_line + 3;
    char branch[64] = "";
    
    // Branch name
    int i = 0;
    while (*p && *p != '.' && *p != ' ' && *p != '[' && i < 63) {
        branch[i++] = *p++;
    }
    branch[i] = '\0';

    // Either no branch or no commits yet
    if (strlen(branch) == 0 || strcmp(branch, "No") == 0) return NULL;

    int ahead = 0, behind = 0;
    
    char *ahead_ptr = strstr(first_line, "ahead ");
    char *behind_ptr = strstr(first_line, "behind ");
    
    if (ahead_ptr) ahead = atoi(ahead_ptr + 6);
    if (behind_ptr) behind = atoi(behind_ptr + 7);

    char *info = memalloc(128);
    info[0] = '\0';
    
    strcat(info, branch);
    
    if (ahead > 0) {
        char tmp[32];
        sprintf(tmp, " %d^", ahead);
        strcat(info, tmp);
    }
    if (behind > 0) {
        char tmp[32];
        sprintf(tmp, " %dv", behind);
        strcat(info, tmp);
    }
    if (is_dirty) {
        strcat(info, " *");
    }

    return info;
}
