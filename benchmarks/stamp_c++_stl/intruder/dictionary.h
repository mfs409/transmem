/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <vector>

struct dictionary_t
{
    std::vector<char*>* stuff;

    dictionary_t();
    ~dictionary_t();
    void add(char* str);
    char* get(long i);
    char* match(char* str);


};

extern const char* global_defaultSignatures[];
extern const long global_numDefaultSignature;
