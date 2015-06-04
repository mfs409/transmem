/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

#include <unordered_set>
#include "segments.h"
#include "table.h"

struct endInfoEntry_t;
struct constructEntry_t;

struct sequencer_hash
{
    __attribute__((transaction_safe))
    size_t operator()(const char*) const noexcept;
};

struct sequencer_compare
{
    __attribute__((transaction_safe))
    bool operator()(const char* a, char* b) const;
};

struct sequencer_t {
    char* sequence;

    segments_t* segmentsPtr;

    /* For removing duplicate segments */
    // [mfs] Replace with std::unordered_set
    std::unordered_set<char*, sequencer_hash, sequencer_compare>* uniqueSegmentsPtr;

    /* For matching segments */
    endInfoEntry_t* endInfoEntries;
    table_t** startHashToConstructEntryTables;

    /* For constructing sequence */
    constructEntry_t* constructEntries;
    table_t* hashToConstructEntryTable;

    /* For deallocation */
    long segmentLength;

    sequencer_t(long geneLength, long segmentLength, segments_t* segmentsPtr);

    ~sequencer_t();
};

struct sequencer_run_arg_t {
    sequencer_t* sequencerPtr;
    segments_t* segmentsPtr;
    long preAllocLength;
    char* returnSequence; /* variable stores return value */
};

/* =============================================================================
 * sequencer_run
 * =============================================================================
 */
void sequencer_run(void* argPtr);
