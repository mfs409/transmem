/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include "error.h"
#include "preprocessor.h"
#include <vector>
#include "dictionary.h"

struct detector_t {
    dictionary_t* dictionaryPtr;
    std::vector<preprocessor_t>* preprocessorVectorPtr;

    detector_t();
    ~detector_t();
    void addPreprocessor(preprocessor_t p);
    int_error_t process(char* str);
};
