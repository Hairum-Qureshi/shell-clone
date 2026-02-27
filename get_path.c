#include "get_path.h"

char *path;  // Global variable to store path

struct pathelement *get_path() {
    char *p;
    struct pathelement *tmp, *pathlist = NULL;

    p = getenv("PATH");
    path = malloc((strlen(p)+1) * sizeof(char)); // Store in global variable
    strncpy(path, p, strlen(p));
    path[strlen(p)] = '\0';

    p = strtok(path, ":");
    do {
        if (!pathlist) {
            tmp = calloc(1, sizeof(struct pathelement));
            pathlist = tmp;
        } else {
            tmp->next = calloc(1, sizeof(struct pathelement));
            tmp = tmp->next;
        }
        tmp->element = p;
        tmp->next = NULL;
    } while (p = strtok(NULL, ":"));

    return pathlist;
}
