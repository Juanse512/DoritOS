#include "syscall.h"
#include "lib.h"

unsigned
strlen(const char *s)
{
    unsigned n;

    for (n = 0; *s != '\0'; s++)
        n++;
    return n;
}

void
puts(const char *s)
{
    Write(s, strlen(s), CONSOLE_OUTPUT);
}

int pow(int base, int exp)
{
    int result = 1;
    for(int i = 0; i < exp; i++)
    {
        result *= base;
    }
    return result;
}

void
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
    int i = sign < 0;
    if(i){
        str[0] = '-';
    }
    for(; i < digits; i++)
    {
        str[i] = (n / (pow(10, (digits - i)))) + '0';
    }
    str[i] = '\0';
}