#include "syscall.h"

static inline unsigned
strlen(const char *s)
{
    unsigned n;

    for (n = 0; *s != '\0'; s++)
        n++;
    return n;
}

static inline void
puts(const char *s)
{
    Write(s, strlen(s), CONSOLE_OUTPUT);
}

static inline int pow(int base, int exp)
{
    int result = 1;
    for(int i = 0; i < exp; i++)
    {
        result *= base;
    }
    return result;
}

static inline void
itoa(int n, char *str)
{
    int i, sign;

    int div = n;
    int digits = 0;
    do{
        div /= 10;
        digits++;
    } while(div != 0);

    if ((sign = n) < 0)
        n = -n;
    i = sign < 0;
    if(i){
        str[0] = '-';
    }
    for(; i < digits; i++)
    {
        str[i] = (n / (int)pow(10, (digits - i - 1))) % 10 + '0';
    }
    str[i] = '\0';
}