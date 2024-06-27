/// Copyright (c) 2018-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "utility.hh"
#include "string.h"

Debug debug;

const char * filepath(char *path, const char *name) {
    int separator = -1;
    for (int i = 0; name[i] != '\0'; i++) {
        if (name[i] == '/') {
            separator = i;
        }
    }
    DEBUG('f', "separator: %d\n", separator);
    if (separator == -1) {
        path[0] = '\0';
        return (char *) name;
    }
    for (int i = 0; i <= separator; i++) {
        path[i] = name[i];
    }
    path[separator] = '\0';
    return name + separator + 1;
}