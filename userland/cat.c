#include "syscall.h"

#define ARGC_ERROR    "Error: missing argument."
#define CREATE_ERROR  "Error: could not create file."

int
main(int argc, char *argv[])
{
    if (argc < 2) {
        Write(ARGC_ERROR, sizeof(ARGC_ERROR) - 1, CONSOLE_OUTPUT);
        Exit(1);
    }
    for(int i = 1; i < argc; i++){
        OpenFileId fd = Open(argv[i]);
        // WriteError(argv[i], CONSOLE_OUTPUT);
        if(fd < 0){
            // puts("Cannot open file");
            Write("Cannot open file.", 18, CONSOLE_OUTPUT);
            return 1;
        }
        char buf[1];
        
        while(Read(buf, 1, fd)){
            Write(buf, 1, CONSOLE_OUTPUT);
        }
        Write("\n", 1, CONSOLE_OUTPUT);
        Close(fd);
    }
    return 0;
}
