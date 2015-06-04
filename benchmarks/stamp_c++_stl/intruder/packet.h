/*
 * PLEASE SEE LICENSE FILE FOR LICENSING AND COPYRIGHT INFORMATION
 */

#pragma once

struct packet_t {
    long flowId;
    long fragmentId;
    long numFragment;
    long length;
    char data[];
};


#define PACKET_HEADER_LENGTH (sizeof(packet_t)) /* no data */


/* =============================================================================
 * packet_compareFlowId
 * =============================================================================
 */
__attribute__ ((transaction_safe))
long
packet_compareFlowId (const void* aPtr, const void* bPtr);

struct packet_compareFragmentId
{
  __attribute__((transaction_safe))
  bool operator()(packet_t* left, packet_t* right);
};
