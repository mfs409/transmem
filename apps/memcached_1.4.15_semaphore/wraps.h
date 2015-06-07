// The purpose of this file is to localize all of the hacks that are required
// to provide transaction_safe variants of standard library functions

//
// Safe assembly wrappers for inet.h functions
//
//   The issue here is that inline asm is always unsafe in the current TM
//   spec.  htons and htonl are macros for some asm that converts little
//   endian (host order) to big endian (network order) on x86 systems.  We
//   can't tm_wrap these, because htons() and htonl() aren't actually
//   functions.  But the asm is side-effect-free, so we can create wrapper
//   functions and mark them as pure.
//

#include <arpa/inet.h>

__attribute__((transaction_pure))
inline uint16_t tm_htons(uint16_t hostshort) {
    return htons(hostshort);
}

__attribute__((transaction_pure))
inline uint32_t tm_htonl(uint32_t hostlong) {
    return htonl(hostlong);
}

//
//
//

// [transmem] This is our 'safe' assert function...
#if !defined(NDEBUG)
void tm_assert_internal(const char *filename, int linenum, const char *funcname, const char *sourceline)
{
    // [mfs] This isn't entirely correct... the formatting is wrong for
    // Solaris, and on Linux we are supposed to also print the name of the
    // executable... we can live without such touch-ups for now...
    fprintf(stderr, "%s:%d: %s: Assertion '%s' failed.\n", filename, linenum, funcname, sourceline);
    abort();
}
#endif

// [transmem] This is our 'safe' printf-and-abort function
void tm_msg_and_die(const char* msg)
{
    fprintf(stderr, msg);
    abort();
}

// [transmem] Provide an implementation of memcmp that is transaction_safe
//
// NB: this code was taken from
// http://doxygen.postgresql.org/memcmp_8c_source.html, and is covered by a
// BSD license
int tm_memcmp(const void *s1, const void *s2, size_t n)
{
    if (n != 0) {
        const unsigned char *p1 = s1, *p2 = s2;
        do
        {
            if (*p1++ != *p2++)
                return (*--p1 - *--p2);
        } while (--n != 0);
    }
    return 0;
}

// [transmem] Provide an implementation of memcpy that is transaction_safe
//
// NB: this code was taken from
// https://www.student.cs.uwaterloo.ca/~cs350/common/os161-src-html/memcpy_8c-source.html
void * tm_memcpy(void *dst, const void *src, size_t len)
{
    size_t i;
    /*
     * memcpy does not support overlapping buffers, so always do it
     * forwards. (Don't change this without adjusting memmove.)
     *
     * For speedy copying, optimize the common case where both pointers
     * and the length are word-aligned, and copy word-at-a-time instead
     * of byte-at-a-time. Otherwise, copy by bytes.
     *
     * The alignment logic below should be portable. We rely on
     * the compiler to be reasonably intelligent about optimizing
     * the divides and modulos out. Fortunately, it is.
     */
    if ((uintptr_t)dst % sizeof(long) == 0 &&
        (uintptr_t)src % sizeof(long) == 0 &&
        len % sizeof(long) == 0)
    {
        long *d = dst;
        const long *s = src;
        for (i=0; i<len/sizeof(long); i++) {
            d[i] = s[i];
        }
    }
    else {
        char *d = dst;
        const char *s = src;
        for (i=0; i<len; i++) {
            d[i] = s[i];
        }
    }
    return dst;
}

// [transmem] provide a safe implementation of strlen
//
// NB: this code is taken from
// https://www.student.cs.uwaterloo.ca/~cs350/common/os161-src-html/strlen_8c-source.html
size_t tm_strlen(const char *s)
{
    size_t ret = 0;
    while (s[ret]) {
        ret++;
    }
    return ret;
}

// [transmem] provide a safe implementation of strncmp
//
// NB: this code is taken from
// http://www.ethernut.de/api/strncmp_8c_source.html
int tm_strncmp(const char *s1, const char *s2, size_t n)
{
    if (n == 0)
        return (0);
    do {
        if (*s1 != *s2++)
            return (*(unsigned char *) s1 - *(unsigned char *) --s2);
        if (*s1++ == 0)
            break;
    } while (--n != 0);
    return (0);
}

// [transmem] Provide a safe strncpy function
//
// NB: this code is taken from
// http://www.ethernut.de/api/strncpy_8c_source.html
char *tm_strncpy(char *dst, const char *src, size_t n)
{
    if (n != 0) {
        char *d = dst;
        const char *s = src;
        do {
            if ((*d++ = *s++) == 0) {
                /* NUL pad the remaining n-1 bytes */
                while (--n) {
                    *d++ = 0;
                }
                break;
            }
        } while (--n);
    }
    return dst;
}

// [transmem] Provide a transaction-safe version of realloc
void *tm_realloc(void *ptr, size_t size, size_t old_size)
{
    void *oldp = ptr;
    void* newp = malloc(size);
    if (newp == NULL)
        return NULL;
    size_t copySize = *(size_t *)((char *)oldp - sizeof(size_t));
    if (size < copySize)
        copySize = size;
    if (old_size > 0) {
        tm_memcpy(newp, oldp, old_size);
    }
    free(oldp);
    return newp;
}

// [transmem] Provide a safe version of strchr.
// This code taken from http://clc-wiki.net/wiki/strchr
char *tm_strchr(const char *s, int c)
{
    while (*s != (char)c)
        if (!*s++)
            return 0;
    return (char *)s;
}

// [transmem] Safe isspace
int tm_isspace(int c)
{
    return isspace(c);
}

// [transmem] Provide a pure memcpy, so that we can do calle stack -to-
//               caller stack transfers without having the writes getting
//               buffered by the TM
__attribute__((transaction_pure))
static void *pure_memcpy(void *dest, const void *src, size_t n)
{
    return memcpy(dest, src, n);
}

// [transmem] Provide a safe strncpy function that copies its argument to
//               a local array instead of to a possibly-shared array
//
// NB: this code is taken from
// http://www.ethernut.de/api/strncpy_8c_source.html
#define MAX(x, y) ((x)>(y)) ? (x) : (y)
char *tm_strncpy_to_local(char *local_dst, const char *src, size_t n)
{
    // keep track of the size, and have a local destination so that gcc
    // doesn't do transactional writes, only transactional reads
    int size = n;
    char dst[MAX(KEY_MAX_LENGTH + 1, STAT_VAL_LEN) + 1];

    if (n != 0) {
        char *d = dst;
        const char *s = src;
        do {
            if ((*d++ = *s++) == 0) {
                /* NUL pad the remaining n-1 bytes */
                while (--n) {
                    *d++ = 0;
                }
                break;
            }
        } while (--n);
    }

    // now we need to copy from dst (which is local to this function) to
    // local_dst (which is local to the caller)
    pure_memcpy(local_dst, dst, size);

    return local_dst;
}

// [transmem] Safe strtol via marshalling
__attribute__((transaction_pure))
static long int pure_strtol(const char *nptr, char **endptr, int base)
{
    return strtol(nptr, endptr, base);
}
// NB: memcached always sends NULL for endptr, so we don't have to do
// anything fancy here
long int tm_strtol(const char *nptr, char **endptr, int base)
{
    // marshall string into buffer... the biggest 64-bit int is
    // 9,223,372,036,854,775,807, so 19 characters.  Note that the string can
    // have a ton of leading whitespace, so we'll go with 10000 to play it
    // safe
    char buf[10000];
    tm_strncpy_to_local(buf, nptr, 4096);
    // now go to a pure function call
    return pure_strtol(buf, endptr, base);
}

// [transmem] Since uses of strtoull need endptr, we re-produce the code
// instead of marshalling.  Code is taken from
// http://ftp.cc.uoc.gr/mirrors/OpenBSD/src/lib/libc/stdlib/strtoull.c
unsigned long long int tm_strtoull(const char *nptr, char **endptr, int base)
{
    const char *s;
    unsigned long long acc, cutoff;
    int c;
    int neg, any, cutlim;

    /*
     * See strtoq for comments as to the logic used.
     */
    s = nptr;
    do {
        c = (unsigned char) *s++;
    } while (isspace(c));
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }
    if ((base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X')) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;

    cutoff = ULLONG_MAX / (unsigned long long)base;
    cutlim = ULLONG_MAX % (unsigned long long)base;
    for (acc = 0, any = 0;; c = (unsigned char) *s++) {
        if (isdigit(c))
            c -= '0';
        else if (isalpha(c))
            c -= isupper(c) ? 'A' - 10 : 'a' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0)
            continue;
        if (acc > cutoff || (acc == cutoff && c > cutlim)) {
            any = -1;
            acc = ULLONG_MAX;
            errno = ERANGE;
        } else {
            any = 1;
            acc *= (unsigned long long)base;
            acc += c;
        }
    }
    if (neg && any > 0)
        acc = -acc;
    if (endptr != 0)
        *endptr = (char *) (any ? s - 1 : nptr);
    return (acc);
}

// [transmem] This is how BSD does atoi...
int tm_atoi(const char *nptr)
{
    return((int)tm_strtol(nptr, (char **)NULL, 10));
}

// [transmem] Provide support for oncommit handlers
void registerOnCommitHandler(void (*func)(void*), void *param)
{
    if (_ITM_inTransaction() == outsideTransaction) {
        func(param);
    }
    else {
        _ITM_addUserCommitAction((_ITM_userCommitFunction)func,
                                 //_ITM_getTransactionId(),
                                 1,
                                 param);
    }
}

// [transmem] This is a mechanism for delaying perror
void delayed_perror(int error_number, char *message)
{
    char buf[4096];
    strerror_r(error_number, buf, 4096);
    fprintf(stderr, "%s: %s\n", message, buf);
}

// [transmem] We need a min function to get append_ascii_stats to work
//            correctly
#define min(x, y) (x) < (y) ? (x) : (y)

// [transmem] Custom pure snprintf for writing "END"
__attribute__((transaction_pure))
static size_t tm_snprintf_end(char *c, size_t s)
{
    return snprintf(c, s, "END\r\n");
}

// [transmem] Forward declare this one
__attribute__((transaction_pure))
static int tm_snprintf_s(char *str, size_t size, const char *format,
                         const char *val);

// [transmem] A second one with two strings
__attribute__((transaction_pure))
static int tm_snprintf_s_s(char *str, size_t size, const char *format,
                           const char *val1, const char *val2)
{
    return snprintf(str, size, format, val1, val2);
}

// [transmem] This form of snprintf is for when we need to marshall onto a
//               shared string in do_add_delta.
__attribute__((transaction_safe))
static void tm_snprintf_dad(char *str, unsigned long long val1)
{
    // create temp space
    char buf[INCR_MAX_STORAGE_LEN];
    // wrapped pure snprintf into space
    tm_snprintf_llu(buf, INCR_MAX_STORAGE_LEN, "%llu", val1);
    // marshall the result into str
    tm_strncpy(str, buf, INCR_MAX_STORAGE_LEN);
}

// [transmem] Wrapped versions of snprintf to make it safe when doing all
//            writes with local vars
int tm_snprintf_llu(char *str, size_t size, const char *format,
                    unsigned long long val)
{
    return snprintf(str, size, format, val);
}
int tm_snprintf_u(char *str, size_t size, const char *format,
                  unsigned int val)
{
    return snprintf(str, size, format, val);
}
int tm_snprintf_d(char *str, size_t size, const char *format, int val)
{
    return snprintf(str, size, format, val);
}
// [transmem] NB: this is just for the b branch
int tm_snprintf_d_d(char *str, size_t size, const char *format, int val1, int val2)
{
    return snprintf(str, size, format, val1, val2);
}
int tm_snprintf_ds(char *str, size_t size, const char *format, int val1,
                   const char *val2)
{
    return snprintf(str, size, format, val1, val2);
}
int tm_snprintf_s_d_lu(char *str, size_t size, const char *format,
                       const char *val1, int val2, unsigned long val3)
{
    return snprintf(str, size, format, val1, val2, val3);
}
int tm_snprintf_s_llu_llu_llu_llu(char *str, size_t size, const char *format,
                                 const char *val1, uint64_t val2, uint64_t val3,
                                 uint64_t val4, uint64_t val5)
{
    return snprintf(str, size, format, val1, val2, val3, val4, val5);
}
__attribute__((transaction_pure))
static int tm_snprintf_lu(char *str, size_t size, const char *format,
                          long val)
{
    return snprintf(str, size, format, val);
}
__attribute__((transaction_pure))
static int tm_snprintf_ld(char *str, size_t size, const char *format,
                          long val)
{
    return snprintf(str, size, format, val);
}
__attribute__((transaction_pure))
static int tm_snprintf_s(char *str, size_t size, const char *format,
                         const char *val)
{
    return snprintf(str, size, format, val);
}
__attribute__((transaction_pure))
static int tm_snprintf_ld_ld(char *str, size_t size, const char *format,
                             long val1, long val2)
{
    return snprintf(str, size, format, val1, val2);
}

// [transmem] This is our 'safe' printf-and-abort function
__attribute__((transaction_pure))
void tm_msg_and_die(const char* msg);

// [branch 009] Provide safe versions of memcmp, memcpy, strlen, strncmp,
//              strncpy, and realloc
__attribute__((transaction_safe))
int tm_memcmp(const void *s1, const void *s2, size_t n);
__attribute__((transaction_safe))
void *tm_memcpy(void *dst, const void *src, size_t len);
__attribute__((transaction_safe))
size_t tm_strlen(const char *s);
__attribute__((transaction_safe))
int tm_strncmp(const char *s1, const char *s2, size_t n);
__attribute__((transaction_safe))
char *tm_strncpy(char *dst, const char *src, size_t n);
__attribute__((transaction_safe))
void *tm_realloc(void *ptr, size_t size, size_t old_size);

// [branch 009b] a custom strncpy that reads via TM, and writes directly to a
//               location presumed to be thread-private (e.g., on the stack)
__attribute__((transaction_safe))
char *tm_strncpy_to_local(char *local_dst, const char *src, size_t n);

// [branch 009b] Provide safe versions of strtol, atoi, strtol, strchr, and
//               isspace
__attribute__((transaction_safe))
long int tm_strtol(const char *nptr, char **endptr, int base);
__attribute__((transaction_safe))
int tm_atoi(const char *nptr);
__attribute__((transaction_safe))
unsigned long long int tm_strtoull(const char *nptr, char **endptr, int base);
__attribute__((transaction_safe))
char *tm_strchr(const char *s, int c);
// [branch 009b] This can just be marked pure
__attribute__((transaction_pure))
int tm_isspace(int c);

// [branch 012] Provide support for oncommit handlers
__attribute__((transaction_pure))
void registerOnCommitHandler(void (*func)(void*), void *param);
void delayed_perror(int error_number, char *message);

// [transmem] Clone append_stat, use safe version
#define APPEND_STAT_LLU(name, fmt, val) \
    append_stat_llu(name, add_stats, c, fmt, val);
#define APPEND_STAT_U(name, fmt, val) \
    append_stat_u(name, add_stats, c, fmt, val);
#define APPEND_STAT_D(name, fmt, val) \
    append_stat_d(name, add_stats, c, fmt, val);
#define APPEND_STAT_LU(name, fmt, val) \
    append_stat_lu(name, add_stats, c, fmt, val);
#define APPEND_STAT_LD(name, fmt, val) \
    append_stat_ld(name, add_stats, c, fmt, val);
#define APPEND_STAT_S(name, fmt, val) \
    append_stat_s(name, add_stats, c, fmt, val);

// [transmem] Clone APPEND_NUM_FMT_STAT to make it safe
#define APPEND_NUM_FMT_STAT_U(name_fmt, num, name, fmt, val)          \
    klen = tm_snprintf_ds(key_str, STAT_KEY_LEN, name_fmt, num, name);    \
    vlen = tm_snprintf_u(val_str, STAT_VAL_LEN, fmt, val);               \
    add_stats(key_str, klen, val_str, vlen, c);
#define APPEND_NUM_FMT_STAT_LLU(name_fmt, num, name, fmt, val)          \
    klen = tm_snprintf_ds(key_str, STAT_KEY_LEN, name_fmt, num, name);    \
    vlen = tm_snprintf_llu(val_str, STAT_VAL_LEN, fmt, val);               \
    add_stats(key_str, klen, val_str, vlen, c);

// [transmem] Clone APPEND_NUM_STAT to make it safe
#define APPEND_NUM_STAT_U(num, name, fmt, val) \
    APPEND_NUM_FMT_STAT_U("%d:%s", num, name, fmt, val)
#define APPEND_NUM_STAT_LLU(num, name, fmt, val) \
    APPEND_NUM_FMT_STAT_LLU("%d:%s", num, name, fmt, val)

// [transmem] safe clones of append_stat without varargs
__attribute__((transaction_safe))
void append_stat_llu(const char *name, ADD_STAT add_stats, conn *c,
                     const char *fmt, unsigned long long val);
__attribute__((transaction_safe))
void append_stat_u(const char *name, ADD_STAT add_stats, conn *c,
                   const char *fmt, unsigned int val);
__attribute__((transaction_safe))
void append_stat_d(const char *name, ADD_STAT add_stats, conn *c,
                   const char *fmt, int val);
__attribute__((transaction_safe))
void append_stat_lu(const char *name, ADD_STAT add_stats, conn *c,
                    const char *fmt, long val);
__attribute__((transaction_safe))
void append_stat_ld(const char *name, ADD_STAT add_stats, conn *c,
                    const char *fmt, long val);
__attribute__((transaction_safe))
void append_stat_s(const char *name, ADD_STAT add_stats, conn *c,
                   const char *fmt, const char *val);
__attribute__((transaction_safe))
void append_stat_ld_ld(const char *name, ADD_STAT add_stats, conn *c,
                       const char *fmt, long val1, long val2);
// [transmem] These internal stat helper functions are useful in other
//              places, so they shouldn't be static
__attribute__((transaction_pure))
int tm_snprintf_d(char *str, size_t size, const char *format, int val);
__attribute__((transaction_pure))
int tm_snprintf_llu(char *str, size_t size, const char *format,
                    unsigned long long val);
__attribute__((transaction_pure))
int tm_snprintf_u(char *str, size_t size, const char *format,
                  unsigned int val);
__attribute__((transaction_pure))
int tm_snprintf_ds(char *str, size_t size, const char *format,
                   int val1, const char *val2);
// [transmem] A snprintf variant for do_item_cachedump
__attribute__((transaction_pure))
int tm_snprintf_s_d_lu(char *str, size_t size, const char *format,
                       const char *val1, int val2, unsigned long val3);
__attribute__((transaction_pure))
int tm_snprintf_s_llu_llu_llu_llu(char *str, size_t size, const char *format,
                                 const char *val1, uint64_t val2, uint64_t val3,
                                 uint64_t val4, uint64_t val5);
// [transmem] This one is just for the b branch
__attribute__((transaction_pure))
int tm_snprintf_d_d(char *str, size_t size, const char *format, int val1, int val2);

// [transmem] This function is called from a relaxed transaction
// [transmem] With safe string functions, this becomes safe
__attribute__((transaction_safe))
bool safe_strtoull(const char *str, uint64_t *out);
bool safe_strtoll(const char *str, int64_t *out);
bool safe_strtoul(const char *str, uint32_t *out);
bool safe_strtol(const char *str, int32_t *out);
