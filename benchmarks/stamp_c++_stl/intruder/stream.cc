/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "detector.h"
#include "dictionary.h"
#include "packet.h"
#include "stream.h"

stream_t::stream_t(long _percentAttack)
{
    assert(_percentAttack >= 0 && _percentAttack <= 100);
    percentAttack = _percentAttack;
    randomPtr = new std::mt19937();
    assert(randomPtr);
    allocVectorPtr = new std::vector<char*>();
    assert(allocVectorPtr);
    packetQueuePtr = new std::queue<packet_t*>();
    assert(packetQueuePtr);
    attackMapPtr = new std::map<long, char*>();
    assert(attackMapPtr);
}


stream_t::~stream_t()
{
    long numAlloc = allocVectorPtr->size();

    for (long a = 0; a < numAlloc; a++) {
        char* str = allocVectorPtr->at(a);
        free(str);
    }

    delete attackMapPtr;
    delete packetQueuePtr;
    delete allocVectorPtr;
    delete randomPtr;
}


/* =============================================================================
 * splitIntoPackets
 * -- Packets will be equal-size chunks except for last one, which will have
 *    all extra bytes
 * =============================================================================
 */
static void splitIntoPackets(char* str,
                             long flowId,
                             std::mt19937* randomPtr,
                             std::vector<char*>* allocVectorPtr,
                             std::queue<packet_t*>* packetQueuePtr)
{
    long numByte = strlen(str);
    long numPacket = randomPtr->operator()() % numByte + 1;

    long numDataByte = numByte / numPacket;

    long p;
    for (p = 0; p < (numPacket - 1); p++) {
        char* bytes = (char*)malloc(PACKET_HEADER_LENGTH + numDataByte);
        assert(bytes);
        allocVectorPtr->push_back(bytes);
        packet_t* packetPtr = (packet_t*)bytes;
        packetPtr->flowId      = flowId;
        packetPtr->fragmentId  = p;
        packetPtr->numFragment = numPacket;
        packetPtr->length      = numDataByte;
        memcpy(packetPtr->data, (str + p * numDataByte), numDataByte);
        packetQueuePtr->push(packetPtr);
    }

    long lastNumDataByte = numDataByte + numByte % numPacket;
    char* bytes = (char*)malloc(PACKET_HEADER_LENGTH + lastNumDataByte);
    assert(bytes);
    allocVectorPtr->push_back(bytes);
    packet_t* packetPtr = (packet_t*)bytes;
    packetPtr->flowId      = flowId;
    packetPtr->fragmentId  = p;
    packetPtr->numFragment = numPacket;
    packetPtr->length      = lastNumDataByte;
    memcpy(packetPtr->data, (str + p * numDataByte), lastNumDataByte);
    packetQueuePtr->push(packetPtr);
}


/* =============================================================================
 * stream_generate
 * -- Returns number of attacks generated
 * =============================================================================
 */
long stream_t::generate(dictionary_t* dictionaryPtr,
                        long numFlow,
                        long seed,
                        long maxLength)
{
    long numAttack = 0;

    detector_t* detectorPtr = new detector_t();
    assert(detectorPtr);
    detectorPtr->addPreprocessor(&preprocessor_toLower);

    randomPtr->seed(seed);
    while (!packetQueuePtr->empty())
        packetQueuePtr->pop();

    long range = '~' - ' ' + 1;
    assert(range > 0);

    long f;
    for (f = 1; f <= numFlow; f++) {
        char* str;
        //[wer210] added cast to long
        if ((long)(randomPtr->operator()() % 100) < percentAttack) {
            long s = randomPtr->operator()() % global_numDefaultSignature;
            str = dictionaryPtr->get(s);
            attackMapPtr->insert(std::make_pair(f, str));
            numAttack++;
        } else {
            /*
             * Create random string
             */
            long length = (randomPtr->operator()() % maxLength) + 1;
            str = (char*)malloc((length + 1) * sizeof(char));
            allocVectorPtr->push_back(str);
            long l;
            for (l = 0; l < length; l++) {
                str[l] = ' ' + (char)(randomPtr->operator()() % range);
            }
            str[l] = '\0';
            char* str2 = (char*)malloc((length + 1) * sizeof(char));
            assert(str2);
            strcpy(str2, str);
            int_error_t error = detectorPtr->process(str2); /* updates in-place */
            if (error == ERROR_SIGNATURE) {
                attackMapPtr->insert(std::make_pair(f, str));
                numAttack++;
            }
            free(str2);
        }
        splitIntoPackets(str, f, randomPtr, allocVectorPtr, packetQueuePtr);
    }

    // shuffle the queue:
    {
        // get the size of the queue
        long numElement = packetQueuePtr->size();

        // move queue elements into a vector
        std::vector<packet_t*> ev;
        while (!packetQueuePtr->empty()) {
            ev.push_back(packetQueuePtr->front());
            packetQueuePtr->pop();
        }
        assert(packetQueuePtr->empty());

        // shuffle the vector
        for (long i = 0; i < numElement; i++) {
            long r1 = randomPtr->operator()() % numElement;
            long r2 = randomPtr->operator()() % numElement;
            packet_t* tmp = ev[r1];
            ev[r1] = ev[r2];
            ev[r2] = tmp;
        }

        // move data back into the queue
        for (auto i : ev) {
            packetQueuePtr->push(i);
        }
    }

    delete detectorPtr;

    return numAttack;
}



/* =============================================================================
 * stream_getPacket
 * -- If none, returns NULL
 * =============================================================================
 */
__attribute__((transaction_safe))
char* stream_t::getPacket()
{
    if (packetQueuePtr->empty())
        return NULL;
    char* res = (char*)packetQueuePtr->front();
    packetQueuePtr->pop();
    return res;
}


/* =============================================================================
 * stream_isAttack
 * =============================================================================
 */
bool stream_t::isAttack(long flowId)
{
    return attackMapPtr->find(flowId) != attackMapPtr->end();
}


/* #############################################################################
 * TEST_STREAM
 * #############################################################################
 */
#ifdef TEST_STREAM


#include <assert.h>
#include <stdio.h>


int
main ()
{
    long percentAttack = 10;
    long numFlow = 100;
    long seed = 0;
    long maxLength = 20;

    puts("Starting...");

    stream_t* streamPtr = stream_alloc(percentAttack);
    assert(streamPtr);

    dictionary_t* dictionaryPtr = dictionary_alloc();
    assert(dictionaryPtr);

    stream_generate(streamPtr, dictionaryPtr, numFlow, seed, maxLength);

    char* bytes;
    while ((bytes = stream_getPacket(streamPtr))) {
        packet_t* packetPtr = (packet_t*)bytes;
        long  flowId      = packetPtr->flowId;
        long  fragmentId  = packetPtr->fragmentId;
        long  numFragment = packetPtr->numFragment;
        long  length      = packetPtr->length;
        char* data        = packetPtr->data;
        long l;
        printf("flow=%2li frag=%2li num=%2li data=", flowId, fragmentId, numFragment);
        for (l = 0; l < length; l++) {
            printf("%c", data[l]);
        }
        puts("");
    }

    stream_free(streamPtr);

    puts("Done.");

    return 0;
}


#endif /* TEST_STREAM */
