/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

/*
 * gene.c: Create random gene
 */

#include <assert.h>
#include <stdlib.h>
#include <random>
#include "gene.h"
#include "nucleotide.h"

/* =============================================================================
 * gene_alloc
 * -- Does all memory allocation necessary for gene creation
 * -- Returns NULL on failure
 * =============================================================================
 */
gene_t::gene_t(long _length)
{
    assert(_length > 1);
    contents = (char*)malloc((_length + 1) * sizeof(char));
    assert(contents != NULL);

    contents[_length] = '\0';
    length = _length;

    startBitmapPtr = new std::vector<bool>(_length, false);
    assert(startBitmapPtr != NULL);
}


/* =============================================================================
 * gene_create
 * -- Populate contents with random gene
 * =============================================================================
 */
void gene_t::create(std::mt19937* randomPtr)
{
    const char nucleotides[] = {
        NUCLEOTIDE_ADENINE,
        NUCLEOTIDE_CYTOSINE,
        NUCLEOTIDE_GUANINE,
        NUCLEOTIDE_THYMINE,
    };

    assert(randomPtr != NULL);

    for (long i = 0; i < length; i++) {
        contents[i] = nucleotides[(randomPtr->operator()()% NUCLEOTIDE_NUM_TYPE)];
    }
}


/* =============================================================================
 * gene_free
 * =============================================================================
 */
gene_t::~gene_t()
{
    delete startBitmapPtr;
    free(contents);
}


/* =============================================================================
 * TEST_GENE
 * =============================================================================
 */
#ifdef TEST_GENE


#include <assert.h>
#include <stdio.h>
#include <string.h>


int
main ()
{
    gene_t* gene1Ptr;
    gene_t* gene2Ptr;
    gene_t* gene3Ptr;
    random_t* randomPtr;

    bool status = memory_init(1, 4, 2);
    assert(status);

    puts("Starting...");

    gene1Ptr = gene_alloc(10);
    gene2Ptr = gene_alloc(10);
    gene3Ptr = gene_alloc(9);
    randomPtr = random_alloc();

    random_seed(randomPtr, 0);
    gene_create(gene1Ptr, randomPtr);
    random_seed(randomPtr, 1);
    gene_create(gene2Ptr, randomPtr);
    random_seed(randomPtr, 0);
    gene_create(gene3Ptr, randomPtr);

    assert(gene1Ptr->length == strlen(gene1Ptr->contents));
    assert(gene2Ptr->length == strlen(gene2Ptr->contents));
    assert(gene3Ptr->length == strlen(gene3Ptr->contents));

    assert(gene1Ptr->length == gene2Ptr->length);
    assert(strcmp(gene1Ptr->contents, gene2Ptr->contents) != 0);

    assert(gene1Ptr->length == (gene3Ptr->length + 1));
    assert(strcmp(gene1Ptr->contents, gene3Ptr->contents) != 0);
    assert(strncmp(gene1Ptr->contents,
                   gene3Ptr->contents,
                   gene3Ptr->length) == 0);

    gene_free(gene1Ptr);
    gene_free(gene2Ptr);
    gene_free(gene3Ptr);
    random_free(randomPtr);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_GENE */


/* =============================================================================
 *
 * End of gene.c
 *
 * =============================================================================
 */
