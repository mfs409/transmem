#ifndef HASH_H
#define    HASH_H

#ifdef    __cplusplus
extern "C" {
#endif

// [branch 004] This function is called from a relaxed transaction
// [branch 005] This function is actually transaction safe
__attribute__((transaction_safe))
uint32_t hash(const void *key, size_t length, const uint32_t initval);

#ifdef    __cplusplus
}
#endif

#endif    /* HASH_H */

