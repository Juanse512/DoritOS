/// Outputs arguments entered on the command line.

#include "syscall.h"


unsigned
StringLength(const char *s)
{
    // What if `s` is null?

    unsigned i;
    for (i = 0; s[i] != '\0'; i++) {}
    return i;
}

int
PrintString(const char *s)
{
    // What if `s` is null?

    unsigned len = StringLength(s);
    return Write(s, len, CONSOLE_OUTPUT);
}

int
PrintChar(char c)
{
    return Write(&c, 1, CONSOLE_OUTPUT);
}

int
main(int argc, char *argv[])
{
    char buff[5];
    Read(buff, 4, CONSOLE_INPUT);
    Write(buff, 4, CONSOLE_OUTPUT);

    return 0;
}
