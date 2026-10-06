/*
 * Conformance checks for the strtol family in the posix-uefi fork.
 *
 * strtol decides whether a number in a configuration file is a number, so it
 * matters that it follows the standard rather than approximately follows it.
 * The expectations come from the normative text: base 0 picks the radix from
 * the prefix, a subject sequence that is empty or malformed leaves endptr at
 * the start of the string, and an out of range value saturates and sets
 * ERANGE.
 *
 * Only stdlib.c is linked, and it is compiled whole. The firmware globals its
 * unreached functions reference are stubbed below, and abort/printf resolve to
 * the host libc. The names under test are renamed by the build so they cannot
 * collide with anything the host provides.
 */

#include <uefi.h>

/* Declared here so the test does not have to pull in a host unistd.h. */
extern long write(int fd, const void *buf, unsigned long count);

/*
 * stdlib.c brings in the whole standard library, and the parts that talk to
 * the firmware need these globals to link. Nothing here is ever reached: the
 * test only calls the strtol family. errno is defined by stdlib.c itself.
 */
efi_handle_t IM;
efi_system_table_t *ST;
efi_boot_services_t *BS;
efi_runtime_services_t *RT;
efi_loaded_image_protocol_t *LIP;
char *__argvutf8;
void __stdio_cleanup(void) { }

static int failures;
static int checks;

static void say(const char *s)
{
    unsigned long n = 0;
    while(s[n] != '\0') n++;
    write(1, s, n);
}

static void sayNum(long long v)
{
    char buf[24];
    int i = (int)sizeof(buf);
    unsigned long long u = v < 0 ? (unsigned long long)(-v) : (unsigned long long)v;

    buf[--i] = '\0';
    do {
        buf[--i] = (char)('0' + (int)(u % 10));
        u /= 10;
    } while(u != 0);
    if(v < 0) buf[--i] = '-';
    say(&buf[i]);
}

static void sayUnum(unsigned long long u)
{
    char buf[24];
    int i = (int)sizeof(buf);

    buf[--i] = '\0';
    do {
        buf[--i] = (char)('0' + (int)(u % 10));
        u /= 10;
    } while(u != 0);
    say(&buf[i]);
}

static void report(const char *what, const char *input, long long want, long long got)
{
    say("FAIL ");
    say(what);
    say(" \"");
    say(input);
    say("\" want ");
    sayNum(want);
    say(" got ");
    sayNum(got);
    say("\n");
}

static void reportU(const char *what, const char *input, unsigned long long want, unsigned long long got)
{
    say("FAIL ");
    say(what);
    say(" \"");
    say(input);
    say("\" want ");
    sayUnum(want);
    say(" got ");
    sayUnum(got);
    say("\n");
}

static void checkInt(const char *input, long long got, long long want)
{
    checks++;
    if(got != want) {
        failures++;
        report("value", input, want, got);
    }
}

static void checkUint(const char *input, unsigned long long got, unsigned long long want)
{
    checks++;
    if(got != want) {
        failures++;
        reportU("value", input, want, got);
    }
}

static void checkErrno(const char *input, int got, int want)
{
    checks++;
    if(got != want) {
        failures++;
        say("FAIL errno for \"");
        say(input);
        say("\"\n");
    }
}

/* The offset of the first unconverted character, -1 when endptr points at the
 * start of the input, -2 when it is NULL. */
static void checkEnd(const char *input, char *end, long long want)
{
    long long offset = end == NULL ? -2 : (long long)(end - (char *)input);
    checks++;
    if(offset != want) {
        failures++;
        report("endptr", input, want, offset);
    }
}

static void T(const char *input, int base, long long wantValue, int wantErrno, long long wantOffset)
{
    char buf[64];
    char *end;
    long long got;
    unsigned i = 0;

    while(input[i] != '\0' && i < sizeof(buf) - 1) {
        buf[i] = input[i];
        i++;
    }
    buf[i] = '\0';

    errno = 0;
    end = (char *)0;
    got = (long long)usStrtol(buf, &end, base);

    checkInt(input, got, wantValue);
    checkErrno(input, errno, wantErrno);
    checkEnd(buf, end, wantOffset);
}

static void TU(const char *input, int base, unsigned long long wantValue, int wantErrno, long long wantOffset)
{
    char buf[64];
    char *end;
    unsigned long long got;
    unsigned i = 0;

    while(input[i] != '\0' && i < sizeof(buf) - 1) {
        buf[i] = input[i];
        i++;
    }
    buf[i] = '\0';

    errno = 0;
    end = (char *)0;
    got = usStrtoull(buf, &end, base);

    checkUint(input, got, wantValue);
    checkErrno(input, errno, wantErrno);
    checkEnd(buf, end, wantOffset);
}

int main(void)
{
    /* Radix selection: base 0 reads the prefix, an explicit base 16 accepts
     * it as well. */
    T("123", 0, 123, 0, 3);
    T("123", 10, 123, 0, 3);
    T("0x1f", 0, 31, 0, 4);
    T("0X1F", 0, 31, 0, 4);
    T("010", 0, 8, 0, 3);
    T("0x1f", 16, 31, 0, 4);
    T("010", 8, 8, 0, 3);
    T("0", 0, 0, 0, 1);
    T("z", 36, 35, 0, 1);
    /* 'z' is 35, which base 35 does not accept, so nothing converts. */
    T("z", 35, 0, 0, 0);

    /* Sign, and the longest valid prefix of a longer string. */
    T("-42", 10, -42, 0, 3);
    T("+42", 10, 42, 0, 3);
    T("  -42abc", 10, -42, 0, 5);
    T("08", 0, 0, 0, 1);
    T("0x", 16, 0, 0, 1);
    T("0x", 0, 0, 0, 1);

    /* Nothing convertible: endptr stays at the start of the string. */
    T("abc", 10, 0, 0, 0);
    T("  abc", 10, 0, 0, 0);
    T("+", 10, 0, 0, 0);
    T("", 10, 0, 0, 0);

    /* Range boundaries are exact; either side of them saturates. */
    T("9223372036854775807", 10, 9223372036854775807LL, 0, 19);
    T("-9223372036854775808", 10, -9223372036854775807LL - 1, 0, 20);
    T("9223372036854775808", 10, 9223372036854775807LL, ERANGE, 19);
    T("-9223372036854775809", 10, -9223372036854775807LL - 1, ERANGE, 20);
    T("99999999999999999999", 10, 9223372036854775807LL, ERANGE, 20);
    T("-99999999999999999999", 10, -9223372036854775807LL - 1, ERANGE, 21);

    /* An unsupported base: zero, EINVAL, and endptr left alone. */
    T("1", 1, 0, EINVAL, -2);
    T("1", 37, 0, EINVAL, -2);
    T("1", -1, 0, EINVAL, -2);

    /* strtoll shares strtol, spot check the extremes. */
    errno = 0;
    checks++;
    if(usStrtoll("-9223372036854775808", NULL, 10) != (-9223372036854775807LL - 1) || errno != 0) {
        failures++;
        say("FAIL strtoll min\n");
    }

    /* strtoull ranges over the whole uint64_t, and a negative input wraps. */
    TU("42", 10, 42, 0, 2);
    TU("0x1f", 0, 31, 0, 4);
    TU("9223372036854775808", 10, 9223372036854775808ULL, 0, 19);
    TU("18446744073709551615", 10, 18446744073709551615ULL, 0, 20);
    TU("18446744073709551616", 10, 18446744073709551615ULL, ERANGE, 20);
    TU("-1", 10, 18446744073709551615ULL, 0, 2);
    TU("-18446744073709551615", 10, 1, 0, 21);
    TU("abc", 10, 0, 0, 0);
    TU("1", 1, 0, EINVAL, -2);

    /* A NULL endptr is allowed. */
    errno = 0;
    checks++;
    if(usStrtol((char *)"42", NULL, 10) != 42 || errno != 0) {
        failures++;
        say("FAIL NULL endptr\n");
    }

    sayNum(checks);
    say(" checks, ");
    sayNum(failures);
    say(" failures\n");
    return failures != 0;
}
