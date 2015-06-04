/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include "dictionary.h"

const char* global_defaultSignatures[] = {
    "about",
    "after",
    "all",
    "also",
    "and",
    "any",
    "back",
    "because",
    "but",
    "can",
    "come",
    "could",
    "day",
    "even",
    "first",
    "for",
    "from",
    "get",
    "give",
    "good",
    "have",
    "him",
    "how",
    "into",
    "its",
    "just",
    "know",
    "like",
    "look",
    "make",
    "most",
    "new",
    "not",
    "now",
    "one",
    "only",
    "other",
    "out",
    "over",
    "people",
    "say",
    "see",
    "she",
    "some",
    "take",
    "than",
    "that",
    "their",
    "them",
    "then",
    "there",
    "these",
    "they",
    "think",
    "this",
    "time",
    "two",
    "use",
    "want",
    "way",
    "well",
    "what",
    "when",
    "which",
    "who",
    "will",
    "with",
    "work",
    "would",
    "year",
    "your"
};

const long global_numDefaultSignature =
    sizeof(global_defaultSignatures) / sizeof(global_defaultSignatures[0]);

dictionary_t::dictionary_t()
{
    stuff = new std::vector<char*>();
    stuff->reserve(global_numDefaultSignature);

    for (long s = 0; s < global_numDefaultSignature; s++) {
        char* sig = const_cast<char*>(global_defaultSignatures[s]);
        stuff->push_back(sig);
    }
}

dictionary_t::~dictionary_t()
{
    delete stuff;
}

void dictionary_t::add(char* str)
{
    stuff->push_back(str);
}

char* dictionary_t::get(long i)
{
    return stuff->at(i);
}

/* =============================================================================
 * dictionary_match
 * =============================================================================
 */
char* dictionary_t::match(char* str)
{
    long s;
    long numSignature = stuff->size();

    for (s = 0; s < numSignature; s++) {
        char* sig = stuff->at(s);
        if (strstr(str, sig) != NULL) {
            return sig;
        }
    }

    return NULL;
}


/* #############################################################################
 * TEST_DICTIONARY
 * #############################################################################
 */
#ifdef TEST_DICTIONARY


#include <assert.h>
#include <stdio.h>


int
main ()
{
    puts("Starting...");

    dictionary_t* dictionaryPtr;

    dictionaryPtr = dictionary_alloc();
    assert(dictionaryPtr);

    assert(dictionary_add(dictionaryPtr, "test1"));
    char* sig = dictionary_match(dictionaryPtr, "test1");
    assert(strcmp(sig, "test1") == 0);
    sig = dictionary_match(dictionaryPtr, "test1s");
    assert(strcmp(sig, "test1") == 0);
    assert(!dictionary_match(dictionaryPtr, "test2"));

    long s;
    for (s = 0; s < global_numDefaultSignature; s++) {
        char* sig = dictionary_match(dictionaryPtr, global_defaultSignatures[s]);
        assert(strcmp(sig, global_defaultSignatures[s]) == 0);
    }

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_DICTIONARY */
