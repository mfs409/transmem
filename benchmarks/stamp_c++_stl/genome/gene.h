/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * gene.h: Create random gene
 */

#pragma once

#include <random>
#include <vector>

struct gene_t {
    long length;
    char* contents;
    // [mfs] replace with std::bitset?
    std::vector<bool>* startBitmapPtr; /* used for creating segments */

    /*
     * gene_alloc: Does all memory allocation necessary for gene creation
     */
    gene_t(long length);

    /*
     * gene_create: Populate contents with random gene
     */
    void create(std::mt19937* randomPtr);

    /*
     * gene_free
     */
    ~gene_t();
};

