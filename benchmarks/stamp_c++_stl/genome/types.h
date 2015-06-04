/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

struct constructEntry_t {
    bool isStart;
    char* segment;
    unsigned long endHash;
    constructEntry_t* startPtr;
    constructEntry_t* nextPtr;
    constructEntry_t* endPtr;
    long overlap;
    long length;
};
