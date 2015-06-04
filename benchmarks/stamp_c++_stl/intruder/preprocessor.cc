/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <string.h>
#include "preprocessor.h"


/* =============================================================================
 * preprocessor_convertURNHex
 * -- Translate % hex escape sequences
 * =============================================================================
 */
void
preprocessor_convertURNHex (char* str)
{
    char* src = str;
    char* dst = str;
    char c;

    while ((c = *src) != '\0') {
        if (c == '%') {
            char hex[3];
            hex[0] = (char)tolower((int)*(src + 1));
            assert(hex[0]);
            hex[1] = (char)tolower((int)*(src + 2));
            assert(hex[1]);
            hex[2] = '\0';
            int i;
            int n = sscanf(hex, "%x", &i);
            assert(n == 1);
            src += 2;
            *src = (char)i;
        }
        *dst = *src;
        src++;
        dst++;
    }

    *dst = '\0';
}


/* =============================================================================
 * preprocessor_toLower
 * -- Translate uppercase letters to lowercase
 * =============================================================================
 */
void
preprocessor_toLower (char* str)
{
    char* src = str;

    while (*src != '\0') {
        *src = (char)tolower((int)*src);
        src++;
    }
}


/* #############################################################################
 * TEST_PREPROCESSOR
 * #############################################################################
 */
#ifdef TEST_PREPROCESSOR


#include <assert.h>
#include <stdio.h>


int
main ()
{
    puts("Starting...");

    char hex[] = "This%20is %41 test%3F%3f";
    preprocessor_convertURNHex(hex);
    assert(strcmp(hex, "This is A test??") == 0);

    char caps[] = "ThiS is A tEsT??";
    preprocessor_toLower(caps);
    assert(strcmp(caps, "this is a test??") == 0);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_PREPROCESSOR */


/* =============================================================================
 *
 * End of preprocessor.c
 *
 * =============================================================================
 */
