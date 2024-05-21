#include "syscall.h"

#define ARGC_ERROR    "Error: missing argument."
#define DELETE_ERROR  "Error: could not delete file."

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        Write(ARGC_ERROR, sizeof(ARGC_ERROR) - 1, CONSOLE_OUTPUT);
        Exit(1);
    }
    int r;
    for(int i = 1; i < argc; i++){
        r = Remove(argv[i]);
        if(r != 0){
            Write(DELETE_ERROR, sizeof(DELETE_ERROR) - 1, CONSOLE_OUTPUT);
            Exit(1);
        }
    }
    return 0;
}
