#include "syscall.h"


int
main(void)
{
    SpaceId    newProc;
    OpenFileId input  = CONSOLE_INPUT;
    OpenFileId output = CONSOLE_OUTPUT;
    char       prompt[2] = { '-', '-' };
    char       ch, buffer[60];
    int        i;

    for (;;) {
        Write(prompt, 2, output);
        i = 0;
        do {
            Read(&buffer[i], 1, input);
        } while (buffer[i++] != '\n');

        buffer[--i] = '\0';
        int dont_wait = (buffer[0] == '&');

        char* arg[1] = {buffer};
        if (i > 0) {
            newProc = Exec(buffer + dont_wait, arg);
            if(!dont_wait)
                Join(newProc);
        }
    }

    return -1;
}
