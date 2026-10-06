/*
 * Format matrix for the printf core in the posix-uefi fork.
 *
 * Every case states the expected text literally, so this file doubles as a
 * readable description of the supported conversions. Only the fork's own
 * formatter is used, never the host one.
 *
 * Host libc headers are deliberately absent: their declarations clash with
 * the freestanding ones in uefi.h. The little reporting this file needs is
 * spelled out locally.
 */

#include <uefi.h>

/* Renamed to usVsnprintf by the build so it cannot shadow the host symbol. */
extern int usVsnprintf(char_t *dst, size_t maxlen, const char_t *fmt, __builtin_va_list args);

/* The one host call used, declared here to avoid pulling in unistd.h. */
extern long write(int fd, const void *buf, unsigned long count);

static int failures;
static int checks;

static void say(const char *s)
{
    unsigned long n = 0;
    while(s[n] != '\0') n++;
    write(1, s, n);
}

static int same(const char *a, const char *b)
{
    while(*a != '\0' && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Renders through the fork's formatter, so every call exercises it. */
static const char *fmt(const char *f, ...)
{
    static char buf[512];
    __builtin_va_list ap;
    __builtin_va_start(ap, f);
    usVsnprintf(buf, sizeof(buf), f, ap);
    __builtin_va_end(ap);
    return buf;
}

static void check(const char *name, const char *want, const char *got)
{
    checks++;
    if(!same(want, got)) {
        failures++;
        say("FAIL ");
        say(name);
        say(": want \"");
        say(want);
        say("\" got \"");
        say(got);
        say("\"\n");
    }
}

#define T(name, want, ...) check(name, want, fmt("" __VA_ARGS__))

int main(void)
{
    T("literal", "abc", "abc");
    T("percent", "100%", "100%%");

    T("decimal", "-42", "%d", -42);
    T("unsigned", "42", "%u", 42u);
    T("plus flag", "+7", "%+d", 7);
    T("space flag", " 7", "% d", 7);
    T("zero pad", "-0042", "%05d", -42);
    T("left align", "42   |", "%-5d|", 42);
    T("right align", "   42|", "%5d|", 42);
    T("precision int", "00042", "%.5d", 42);
    T("zero with precision 0", "", "%.0d", 0);
    T("star width", "   42", "%*d", 5, 42);
    T("star precision", "00042", "%.*d", 5, 42);

    T("hex lower", "deadbeef", "%x", 0xdeadbeefu);
    T("hex upper", "DEADBEEF", "%X", 0xdeadbeefu);
    T("hex alt", "0x2a", "%#x", 42u);
    T("hex alt upper", "0X2A", "%#X", 42u);
    T("hex zero padded", "0000002a", "%08x", 42u);
    T("octal", "52", "%o", 42u);
    T("octal alt", "052", "%#o", 42u);
    T("pointer", "0x0000000000001234", "%p", (void *)0x1234);

    T("string", "hi", "%s", "hi");
    T("string precision", "hi", "%.2s", "hijk");
    T("string width", "    hi|", "%6s|", "hi");
    T("string left", "hi    |", "%-6s|", "hi");
    T("null string", "(null)", "%s", (char *)NULL);
    T("char", "A", "%c", 'A');
    T("char width", "  A", "%3c", 'A');

    T("negative via unsigned long", "ffffffffffffffff", "%lx", (unsigned long)-1);
    T("long long min", "-9223372036854775808", "%lld", -9223372036854775807LL - 1);
    T("unsigned long long max", "18446744073709551615", "%llu", 18446744073709551615ULL);
    T("hh truncation", "-1", "%hhd", 255);
    T("h truncation", "-1", "%hd", 65535);
    T("size_t", "8", "%zu", (size_t)8);

    T("mixed", "[abc:00042:0x2a]", "[%s:%05d:%#x]", "abc", 42, 42u);

    /* Floating point is deliberately unsupported: it emits nothing and must
     * not disturb the arguments that follow. */
    T("float suppressed", "ab", "a%fb", 1.5);
    T("float does not eat args", "a|7", "a%f|%d", 1.5, 7);

    say(fmt("%d checks, %d failures\n", checks, failures));
    return failures != 0;
}
