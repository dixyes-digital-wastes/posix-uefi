#include <uefi.h>

/* The firmware has no locale, so these are plain ASCII tests. Anything above
 * 0x7f is neither a letter nor a digit here. */

#define ASCII_UPPER(c)  ((c) >= 'A' && (c) <= 'Z')
#define ASCII_LOWER(c)  ((c) >= 'a' && (c) <= 'z')
#define ASCII_DIGIT(c)  ((c) >= '0' && (c) <= '9')

int isdigit(int __c)
{
    return ASCII_DIGIT(__c);
}

int isalpha(int __c)
{
    return ASCII_UPPER(__c) || ASCII_LOWER(__c);
}

int isalnum(int __c)
{
    return isalpha(__c) || isdigit(__c);
}

int isxdigit(int __c)
{
    return isdigit(__c) || (__c >= 'a' && __c <= 'f') || (__c >= 'A' && __c <= 'F');
}

int isspace(int __c)
{
    return __c == ' ' || __c == '\t' || __c == '\n' || __c == '\v' || __c == '\f' || __c == '\r';
}

int isupper(int __c)
{
    return ASCII_UPPER(__c);
}

int islower(int __c)
{
    return ASCII_LOWER(__c);
}

int isprint(int __c)
{
    return __c >= 0x20 && __c < 0x7f;
}

int iscntrl(int __c)
{
    return (__c >= 0 && __c < 0x20) || __c == 0x7f;
}

int ispunct(int __c)
{
    return isprint(__c) && !isalnum(__c) && __c != ' ';
}

int isgraph(int __c)
{
    return __c > 0x20 && __c < 0x7f;
}

int tolower(int __c)
{
    return ASCII_UPPER(__c) ? __c + ('a' - 'A') : __c;
}

int toupper(int __c)
{
    return ASCII_LOWER(__c) ? __c - ('a' - 'A') : __c;
}
