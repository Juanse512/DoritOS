#include "syscall.h"
#include "lib.h"

static inline void
WriteError(const char *description, OpenFileId output)
{
    // TODO: how to make sure that `description` is not `NULL`?

    static const char PREFIX[] = "Error: ";
    static const char SUFFIX[] = "\n";

    Write(PREFIX, sizeof PREFIX - 1, output);
    Write(description, strlen(description), output);
    Write(SUFFIX, sizeof SUFFIX - 1, output);
}

int main(int argc, char* argv[]){
    if(argc != 3){
        WriteError("Bad arguments", CONSOLE_OUTPUT);
        return -1;
    }


    if(Create(argv[2]) < 0){
        WriteError("Cannot create file", CONSOLE_OUTPUT);
        return 1;
    }

    OpenFileId fd = Open(argv[1]);
    OpenFileId fd2 = Open(argv[2]);

    if(fd < 0 || fd2 < 0){
        WriteError("Cannot open files", CONSOLE_OUTPUT);
        return 1;
    }


    for(;;){
        char buffer[1];
        int n = Read(buffer, 1, fd);
        if(n <= 0){
            break;
        }
        Write(buffer, n, fd2);
    }

}