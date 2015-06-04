/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include "decoder.h"
#include "detector.h"
#include "dictionary.h"
#include "packet.h"
#include "stream.h"
#include "thread.h"
#include "timer.h"

enum param_types {
    PARAM_ATTACK = (unsigned char)'a',
    PARAM_LENGTH = (unsigned char)'l',
    PARAM_NUM    = (unsigned char)'n',
    PARAM_SEED   = (unsigned char)'s',
    PARAM_THREAD = (unsigned char)'t'
};

enum param_defaults {
    PARAM_DEFAULT_ATTACK = 10,
    PARAM_DEFAULT_LENGTH = 128,
    PARAM_DEFAULT_NUM    = 1 << 18,
    PARAM_DEFAULT_SEED   = 1,
    PARAM_DEFAULT_THREAD = 1
};

long global_params[256] = { /* 256 = ascii limit */ };

static void global_param_init()
{
    global_params[PARAM_ATTACK] = PARAM_DEFAULT_ATTACK;
    global_params[PARAM_LENGTH] = PARAM_DEFAULT_LENGTH;
    global_params[PARAM_NUM]    = PARAM_DEFAULT_NUM;
    global_params[PARAM_SEED]   = PARAM_DEFAULT_SEED;
    global_params[PARAM_THREAD] = PARAM_DEFAULT_THREAD;
}

typedef struct arg {
  /* input: */
    stream_t* streamPtr;
    decoder_t* decoderPtr;
  /* output: */
    std::vector<long>** errorVectors;
} arg_t;


/* =============================================================================
 * displayUsage
 * =============================================================================
 */
static void
displayUsage (const char* appName)
{
    printf("Usage: %s [options]\n", appName);
    puts("\nOptions:                            (defaults)\n");
    printf("    a <UINT>   Percent [a]ttack     (%i)\n", PARAM_DEFAULT_ATTACK);
    printf("    l <UINT>   Max data [l]ength    (%i)\n", PARAM_DEFAULT_LENGTH);
    printf("    n <UINT>   [n]umber of flows    (%i)\n", PARAM_DEFAULT_NUM);
    printf("    s <UINT>   Random [s]eed        (%i)\n", PARAM_DEFAULT_SEED);
    printf("    t <UINT>   Number of [t]hreads  (%i)\n", PARAM_DEFAULT_THREAD);
    exit(1);
}


/* =============================================================================
 * parseArgs
 * =============================================================================
 */
static void
parseArgs (long argc, char* const argv[])
{
    long i;
    long opt;

    opterr = 0;

    while ((opt = getopt(argc, argv, "a:l:n:s:t:")) != -1) {
        switch (opt) {
            case 'a':
            case 'l':
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
 * processPackets
 * =============================================================================
 */
static void
processPackets (void* argPtr)
{
    long threadId = thread_getId();

    stream_t*   streamPtr    = ((arg_t*)argPtr)->streamPtr;
    decoder_t*  decoderPtr   = ((arg_t*)argPtr)->decoderPtr;
    std::vector<long>**  errorVectors = ((arg_t*)argPtr)->errorVectors;

    detector_t* detectorPtr = new detector_t();
    assert(detectorPtr);
    detectorPtr->addPreprocessor(&preprocessor_toLower);

    std::vector<long>* errorVectorPtr = errorVectors[threadId];

    while (1) {

        char* bytes;
        __transaction_atomic {
            bytes = streamPtr->getPacket();
        }
        if (!bytes) {
            break;
        }

        packet_t* packetPtr = (packet_t*)bytes;
        long flowId = packetPtr->flowId;

        int_error_t error;
        __transaction_atomic {
            error = decoderPtr->process(bytes,
                                        (PACKET_HEADER_LENGTH + packetPtr->length));
        }
        //TMprint("2.\n");
        if (error) {
            /*
             * Currently, stream_generate() does not create these errors.
             */
            assert(0);
            errorVectorPtr->push_back(flowId);
        }

        char* data;
        long decodedFlowId;
        __transaction_atomic {
            data = decoderPtr->getComplete(&decodedFlowId);
        }
        //TMprint("3.\n");
        if (data) {
            int_error_t error = detectorPtr->process(data);
            free(data);
            if (error) {
                errorVectorPtr->push_back(decodedFlowId);
            }
        }

    }

    delete detectorPtr;
}


/* =============================================================================
 * main
 * =============================================================================
 */
int main (int argc, char** argv)
{
    /*
     * Initialization
     */
    global_param_init();
    parseArgs(argc, (char** const)argv);
    long numThread = global_params[PARAM_THREAD];

    thread_startup(numThread);

    long percentAttack = global_params[PARAM_ATTACK];
    long maxDataLength = global_params[PARAM_LENGTH];
    long numFlow       = global_params[PARAM_NUM];
    long randomSeed    = global_params[PARAM_SEED];
    printf("Percent attack  = %li\n", percentAttack);
    printf("Max data length = %li\n", maxDataLength);
    printf("Num flow        = %li\n", numFlow);
    printf("Random seed     = %li\n", randomSeed);

    dictionary_t* dictionaryPtr = new dictionary_t();
    assert(dictionaryPtr);
    stream_t* streamPtr = new stream_t(percentAttack);
    assert(streamPtr);
    long numAttack = streamPtr->generate(dictionaryPtr,
                                         numFlow,
                                         randomSeed,
                                         maxDataLength);
    printf("Num attack      = %li\n", numAttack);

    decoder_t* decoderPtr = new decoder_t();
    assert(decoderPtr);

    std::vector<long>** errorVectors = (std::vector<long>**)malloc(numThread * sizeof(std::vector<long>*));
    assert(errorVectors);
    long i;
    for (i = 0; i < numThread; i++) {
        std::vector<long>* errorVectorPtr = new std::vector<long>();
        assert(errorVectorPtr);
        errorVectorPtr->reserve(numFlow);
        errorVectors[i] = errorVectorPtr;
    }

    arg_t arg;
    arg.streamPtr    = streamPtr;
    arg.decoderPtr   = decoderPtr;
    arg.errorVectors = errorVectors;

    /*
     * Run transactions
     */

    TIMER_T startTime;
    TIMER_READ(startTime);

#ifdef OTM
#pragma omp parallel
    {
        processPackets((void*)&arg);
    }

#else
    thread_start(processPackets, (void*)&arg);
#endif
    TIMER_T stopTime;
    TIMER_READ(stopTime);
    printf("Time            = %f\n", TIMER_DIFF_SECONDS(startTime, stopTime));

    /*
     * Check solution
     */

    long numFound = 0;
    for (i = 0; i < numThread; i++) {
        std::vector<long>* errorVectorPtr = errorVectors[i];
        long e;
        long numError = errorVectorPtr->size();
        numFound += numError;
        for (e = 0; e < numError; e++) {
            long flowId = errorVectorPtr->at(e);
            bool status = streamPtr->isAttack(flowId);
            assert(status);
        }
    }
    printf("Num found       = %li\n", numFound);
    assert(numFound == numAttack);

    /*
     * Clean up
     */

    for (i = 0; i < numThread; i++) {
        delete errorVectors[i];
    }
    free(errorVectors);
    delete decoderPtr;
    delete streamPtr;
    delete dictionaryPtr;

    thread_shutdown();

    return 0;
}
