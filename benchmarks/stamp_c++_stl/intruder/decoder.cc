/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "decoder.h"
#include "error.h"
#include <set>
#include "packet.h"
#include "tm_transition.h"

decoder_t::decoder_t()
{
    fragmentedMapPtr = new std::map<long, std::set<packet_t*, packet_compareFragmentId>*>();
    assert(fragmentedMapPtr);
    decodedQueuePtr = new std::queue<decoded_t*>();
    assert(decodedQueuePtr);
}

decoder_t::~decoder_t()
{
    delete decodedQueuePtr;
    delete fragmentedMapPtr;
}

//[wer] this function was problematic to write-back algorithms.
__attribute__((transaction_safe))
int_error_t decoder_t::process(char* bytes, long numByte)
{
    /*
     * Basic error checking
     */
    // [wer210] added cast of type to avoid warnings.
    if (numByte < (long) PACKET_HEADER_LENGTH) {
        return ERROR_SHORT;
    }

    packet_t* packetPtr = (packet_t*)bytes;
    long flowId      = packetPtr->flowId;
    long fragmentId  = packetPtr->fragmentId;
    long numFragment = packetPtr->numFragment;
    long length      = packetPtr->length;

    if (flowId < 0) {
        return ERROR_FLOWID;
    }

    if ((fragmentId < 0) || (fragmentId >= numFragment)) {
        return ERROR_FRAGMENTID;
    }

    if (length < 0) {
        return ERROR_LENGTH;
    }

#if 0
    /*
     * With the above checks, this one is redundant
     */
    if (numFragment < 1) {
        return ERROR_NUMFRAGMENT;
    }
#endif

    /*
     * Add to fragmented map for reassembling
     */

    if (numFragment > 1) {

        std::set<packet_t*, packet_compareFragmentId>* fragmentListPtr = NULL;
        auto x = fragmentedMapPtr->find(flowId);
        if (x != fragmentedMapPtr->end())
            fragmentListPtr = x->second;

        if (fragmentListPtr == NULL) {

        // [wer210] the comparator should be __attribute__((transaction_safe))
          fragmentListPtr = new std::set<packet_t*, packet_compareFragmentId>();
        assert(fragmentListPtr);
        auto res = fragmentListPtr->insert(packetPtr);
        assert(res.second == true);
        auto x = fragmentedMapPtr->insert(std::make_pair(flowId, fragmentListPtr));
        assert(x.second);

      }
      else {
          auto it = fragmentListPtr->begin();
          assert(it != fragmentListPtr->end());

          packet_t* firstFragmentPtr = *it;
          ++it;

          long expectedNumFragment = firstFragmentPtr->numFragment;

        if (numFragment != expectedNumFragment) {
            int num = fragmentedMapPtr->erase(flowId);
            assert(num == 1);
          return ERROR_NUMFRAGMENT;
        }

        auto z = fragmentListPtr->insert(packetPtr);
        assert(z.second);

        /*
         * If we have all the fragments we can reassemble them
         */

        if (fragmentListPtr->size() == numFragment) {

          long numByte = 0;
          long i = 0;
          //TMLIST_ITER_RESET(&it, fragmentListPtr);
          it = fragmentListPtr->begin();

          //while (TMLIST_ITER_HASNEXT(&it, fragmentListPtr)) {
          while (it != fragmentListPtr->end()) {
              packet_t* fragmentPtr = *it;
              ++it;

            assert(fragmentPtr->flowId == flowId);
            if (fragmentPtr->fragmentId != i) {
                int num = fragmentedMapPtr->erase(flowId);
                assert(num == 1);
              return ERROR_INCOMPLETE; /* should be sequential */
            }
            numByte += fragmentPtr->length;
            i++;
          }

          char* data = (char*)malloc(numByte + 1);
          assert(data);
          data[numByte] = '\0';
          char* dst = data;

          //TMLIST_ITER_RESET(&it, fragmentListPtr);
          it = fragmentListPtr->begin();

          //while (TMLIST_ITER_HASNEXT(&it, fragmentListPtr)) {
          while (it != fragmentListPtr->end()) {
            //packet_t* fragmentPtr =
            //  (packet_t*)TMLIST_ITER_NEXT(&it, fragmentListPtr);
              packet_t* fragmentPtr = *it;
              ++it;

              for (long i = 0; i < fragmentPtr->length; i++)
                  dst[i] = fragmentPtr->data[i];
              dst += fragmentPtr->length;

          }
          assert(dst == data + numByte);

          decoded_t* decodedPtr = (decoded_t*)malloc(sizeof(decoded_t));
          assert(decodedPtr);
          decodedPtr->flowId = flowId;
          decodedPtr->data = data;

          decodedQueuePtr->push(decodedPtr);

          delete fragmentListPtr;
          int num = fragmentedMapPtr->erase(flowId);
          assert(num == 1);
        }

      }

    } else {
      /*
       * This is the only fragment, so it is ready
       */
        if (fragmentId != 0) {
          return ERROR_FRAGMENTID;
        }

        char* data = (char*)malloc(length + 1);
        assert(data);
        data[length] = '\0';
        memcpy(data, packetPtr->data, length);

        decoded_t* decodedPtr = (decoded_t*)malloc(sizeof(decoded_t));
        assert(decodedPtr);
        decodedPtr->flowId = flowId;
        decodedPtr->data = data;

        decodedQueuePtr->push(decodedPtr);
    }

    return ERROR_NONE;
}


/* =============================================================================
 * TMdecoder_getComplete
 * -- If none, returns NULL
 * =============================================================================
 */
__attribute__((transaction_safe))
char* decoder_t::getComplete(long* decodedFlowIdPtr)
{
    char* data;
    decoded_t* decodedPtr = NULL;
    if (!decodedQueuePtr->empty()) {
        decodedPtr = decodedQueuePtr->front();
        decodedQueuePtr->pop();
    }

    if (decodedPtr) {
        *decodedFlowIdPtr = decodedPtr->flowId;
        data = decodedPtr->data;
        free(decodedPtr);
    } else {
        *decodedFlowIdPtr = -1;
        data = NULL;
    }

    return data;
}


/* #############################################################################
 * TEST_DECODER
 * #############################################################################
 */
#ifdef TEST_DECODER

#include <stdio.h>


int
main ()
{
    decoder_t* decoderPtr;

    puts("Starting...");

    decoderPtr = decoder_alloc();
    assert(decoderPtr);

    long numDataByte = 3;
    long numPacketByte = PACKET_HEADER_LENGTH + numDataByte;

    char* abcBytes = (char*)malloc(numPacketByte);
    assert(abcBytes);
    packet_t* abcPacketPtr;
    abcPacketPtr = (packet_t*)abcBytes;
    abcPacketPtr->flowId = 1;
    abcPacketPtr->fragmentId = 0;
    abcPacketPtr->numFragment = 2;
    abcPacketPtr->length = numDataByte;
    abcPacketPtr->data[0] = 'a';
    abcPacketPtr->data[1] = 'b';
    abcPacketPtr->data[2] = 'c';

    char* defBytes = (char*)malloc(numPacketByte);
    assert(defBytes);
    packet_t* defPacketPtr;
    defPacketPtr = (packet_t*)defBytes;
    defPacketPtr->flowId = 1;
    defPacketPtr->fragmentId = 1;
    defPacketPtr->numFragment = 2;
    defPacketPtr->length = numDataByte;
    defPacketPtr->data[0] = 'd';
    defPacketPtr->data[1] = 'e';
    defPacketPtr->data[2] = 'f';

    assert(TMdecoder_process(decoderPtr, abcBytes, numDataByte) == ERROR_SHORT);

    abcPacketPtr->flowId = -1;
    assert(TMdecoder_process(decoderPtr, abcBytes, numPacketByte) == ERROR_FLOWID);
    abcPacketPtr->flowId = 1;

    abcPacketPtr->fragmentId = -1;
    assert(TMdecoder_process(decoderPtr, abcBytes, numPacketByte) == ERROR_FRAGMENTID);
    abcPacketPtr->fragmentId = 0;

    abcPacketPtr->fragmentId = 2;
    assert(TMdecoder_process(decoderPtr, abcBytes, numPacketByte) == ERROR_FRAGMENTID);
    abcPacketPtr->fragmentId = 0;

    abcPacketPtr->fragmentId = 2;
    assert(TMdecoder_process(decoderPtr, abcBytes, numPacketByte) == ERROR_FRAGMENTID);
    abcPacketPtr->fragmentId = 0;

    abcPacketPtr->length = -1;
    assert(TMdecoder_process(decoderPtr, abcBytes, numPacketByte) == ERROR_LENGTH);
    abcPacketPtr->length = numDataByte;

    assert(TMdecoder_process(decoderPtr, abcBytes, numPacketByte) == ERROR_NONE);
    defPacketPtr->numFragment = 3;
    assert(TMdecoder_process(decoderPtr, defBytes, numPacketByte) == ERROR_NUMFRAGMENT);
    defPacketPtr->numFragment = 2;

    assert(TMdecoder_process(decoderPtr, abcBytes, numPacketByte) == ERROR_NONE);
    defPacketPtr->fragmentId = 0;
    assert(TMdecoder_process(decoderPtr, defBytes, numPacketByte) == ERROR_INCOMPLETE);
    defPacketPtr->fragmentId = 1;

    long flowId;
    assert(TMdecoder_process(decoderPtr, defBytes, numPacketByte) == ERROR_NONE);
    assert(TMdecoder_process(decoderPtr, abcBytes, numPacketByte) == ERROR_NONE);
    char* str = TMdecoder_getComplete(decoderPtr, &flowId);
    assert(strcmp(str, "abcdef") == 0);
    free(str);
    assert(flowId == 1);

    abcPacketPtr->numFragment = 1;
    assert(TMdecoder_process(decoderPtr, abcBytes, numPacketByte) == ERROR_NONE);
    str = TMdecoder_getComplete(decoderPtr, &flowId);
    assert(strcmp(str, "abc") == 0);
    free(str);
    abcPacketPtr->numFragment = 2;
    assert(flowId == 1);

    str = TMdecoder_getComplete(decoderPtr, &flowId);
    assert(str == NULL);
    assert(flowId == -1);

    decoder_free(decoderPtr);

    free(abcBytes);
    free(defBytes);

    puts("All tests passed.");

    return 0;
}


#endif /* TEST_DECODER */


/* =============================================================================
 *
 * End of decoder.c
 *
 * =============================================================================
 */
