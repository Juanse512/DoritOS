/// Copyright (c) 2018-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "utility.hh"
#include "string.h"

Debug debug;

const char * filepath(char *path, const char *name) {
    int separator = -1;
    for (int i = 0; path[i] != '\0'; i++) {
        if (path[i] == '/') {
            separator = i;
        }
    }
    if (separator == -1) {
        return (char *) name;
    }
    for (int i = 0; i <= separator; i++) {
        path[i] = name[i];
    }
    path[separator + 1] = '\0';
    return name + separator + 1;
}