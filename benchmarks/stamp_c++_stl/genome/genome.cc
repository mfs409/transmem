/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <random>
#include "gene.h"
#include "segments.h"
#include "sequencer.h"
#include "thread.h"
#include "timer.h"

enum param_types {
    PARAM_GENE    = (unsigned char)'g',
    PARAM_NUMBER  = (unsigned char)'n',
    PARAM_SEGMENT = (unsigned char)'s',
    PARAM_THREAD  = (unsigned char)'t'
};

#define PARAM_DEFAULT_GENE    (1L << 14)
#define PARAM_DEFAULT_NUMBER  (1L << 24)
#define PARAM_DEFAULT_SEGMENT (1L << 6)
#define PARAM_DEFAULT_THREAD  (1L)

long global_params[256]; /* 256 = ascii limit */

/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                                (defaults)\n");
    printf("    g <UINT>   Length of [g]ene         (%li)\n", PARAM_DEFAULT_GENE);
    printf("    n <UINT>   Min [n]umber of segments (%li)\n", PARAM_DEFAULT_NUMBER);
    printf("    s <UINT>   Length of [s]egment      (%li)\n", PARAM_DEFAULT_SEGMENT);
    printf("    t <UINT>   Number of [t]hreads      (%li)\n", PARAM_DEFAULT_THREAD);
    puts("");
    puts("The actual number of segments created may be greater than -n");
    puts("in order to completely cover the gene.");
    exit(1);
}

/* =============================================================================
 * setDefaultParams
 * =============================================================================
 */
static void setDefaultParams(void)
{
    global_params[PARAM_GENE]    = PARAM_DEFAULT_GENE;
    global_params[PARAM_NUMBER]  = PARAM_DEFAULT_NUMBER;
    global_params[PARAM_SEGMENT] = PARAM_DEFAULT_SEGMENT;
    global_params[PARAM_THREAD]  = PARAM_DEFAULT_THREAD;
}


/* =============================================================================
 * parseArgs
 * =============================================================================
 */
static void parseArgs (long argc, char* const argv[])
{
    long i;
    long opt;

    opterr = 0;

    setDefaultParams();

    while ((opt = getopt(argc, argv, "g:n:s:t:")) != -1) {
        switch (opt) {
            case 'g':
            case 'n':
            case 's':
            case 't':
                global_params[(unsigned char)opt] = atol(optarg);
                break;
            case '?':
            default:
                opterr++;
                break;
        }
    }

    for (i = optind; i < argc; i++) {
        fprintf(stderr, "Non-option argument: %s\n", argv[i]);
        opterr++;
    }

    if (opterr) {
        displayUsage(argv[0]);
    }
}


/* =============================================================================
 * main
 * =============================================================================
 */
int main (int argc, char** argv)
{
    int result = 0;
    TIMER_T start;
    TIMER_T stop;

    /* Initialization */
    parseArgs(argc, (char** const)argv);

    printf("Creating gene and segments... ");
    fflush(stdout);

    long geneLength = global_params[PARAM_GENE];
    long segmentLength = global_params[PARAM_SEGMENT];
    long minNumSegment = global_params[PARAM_NUMBER];
    long numThread = global_params[PARAM_THREAD];

    thread_startup(numThread);

    std::mt19937* randomPtr = new std::mt19937();
    assert(randomPtr != NULL);
    randomPtr->seed(0);

    gene_t* genePtr = new gene_t(geneLength);
    assert( genePtr != NULL);
    genePtr->create(randomPtr);
    char* gene = genePtr->contents;

    segments_t* segmentsPtr = new segments_t(segmentLength, minNumSegment);
    assert(segmentsPtr != NULL);
    segmentsPtr->create(genePtr, randomPtr);
    sequencer_t* sequencerPtr = new sequencer_t(geneLength, segmentLength, segmentsPtr);
    assert(sequencerPtr != NULL);

    puts("done.");
    printf("Gene length     = %li\n", genePtr->length);
    printf("Segment length  = %li\n", segmentsPtr->length);
    printf("Number segments = %li\n", segmentsPtr->contentsPtr->size());
    fflush(stdout);

    /* Benchmark */
    printf("Sequencing gene... ");
    fflush(stdout);
    TIMER_READ(start);
    thread_start(sequencer_run, (void*)sequencerPtr);
    TIMER_READ(stop);
    puts("done.");
    printf("Time = %lf\n", TIMER_DIFF_SECONDS(start, stop));
    fflush(stdout);

    /* Check result */
    {
        char* sequence = sequencerPtr->sequence;
        result = strcmp(gene, sequence);
        printf("Sequence matches gene: %s\n", (result ? "no" : "yes"));
        if (result) {
            printf("gene     = %s\n", gene);
            printf("sequence = %s\n", sequence);
        }
        fflush(stdout);
        assert(strlen(sequence) >= strlen(gene));
    }

    /* Clean up */
    printf("Deallocating memory... ");
    fflush(stdout);
    delete sequencerPtr;
    delete segmentsPtr;
    delete genePtr;
    delete randomPtr;
    puts("done.");
    fflush(stdout);
    thread_shutdown();
    return result;
}
