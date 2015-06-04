/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <stdlib.h>
#include "detector.h"
#include "error.h"
#include "preprocessor.h"


/* =============================================================================
 * detector_alloc
 * =============================================================================
 */
detector_t::detector_t()
{
    dictionaryPtr = new dictionary_t();
    assert(dictionaryPtr);
    preprocessorVectorPtr = new std::vector<preprocessor_t>();
    assert(preprocessorVectorPtr);
}

/* =============================================================================
 * detector_free
 * =============================================================================
 */
detector_t::~detector_t()
{
    delete dictionaryPtr;
    delete preprocessorVectorPtr;
}


/* =============================================================================
 * detector_addPreprocessor
 * =============================================================================
 */
void detector_t::addPreprocessor(preprocessor_t p)
{
    preprocessorVectorPtr->push_back(p);
}


/* =============================================================================
 * detector_process
 * =============================================================================
 */
int_error_t detector_t::process(char* str)
{
    /*
     * Apply preprocessors
     */
    long numPreprocessor = preprocessorVectorPtr->size();
    for (long p = 0; p < numPreprocessor; p++) {
        preprocessor_t preprocessor = preprocessorVectorPtr->at(p);
        preprocessor(str);
    }

    /*
     * Check against signatures of known attacks
     */

    char* signature = dictionaryPtr->match(str);
    if (signature) {
        return ERROR_SIGNATURE;
    }

    return ERROR_NONE;
}


/* #############################################################################
 * TEST_DETECTOR
 * #############################################################################
 */
#ifdef TEST_DETECTOR


#include <assert.h>
#include <stdio.h>


char str1[] = "test";
char str2[] = "abouts";
char str3[] = "aBoUt";
char str4[] = "%41Bout";


int
main ()
{
    puts("Starting...");

    detector_t* detectorPtr = detector_alloc();

    detector_addPreprocessor(detectorPtr, &preprocessor_convertURNHex);
    detector_addPreprocessor(detectorPtr, &preprocessor_toLower);

    assert(detector_process(detectorPtr, str1) == ERROR_NONE);
    assert(detector_process(detectorPtr, str2) == ERROR_SIGNATURE);
    assert(detector_process(detectorPtr, str3) == ERROR_SIGNATURE);
    assert(detector_process(detectorPtr, str4) == ERROR_SIGNATURE);

    detector_free(detectorPtr);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_DETECTOR */


/* =============================================================================
 *
 * End of detector.c
 *
 * =============================================================================
 */
