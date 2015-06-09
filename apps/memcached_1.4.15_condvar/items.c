/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
#include "memcached.h"
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/signal.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <assert.h>

/* Forward Declarations */
// [branch 004] This function is called from a relaxed transaction
// [branch 008] With safe assertions, this becomes safe
__attribute__((transaction_safe))
static void item_link_q(item *it);
// [branch 004] This function is called from a relaxed transaction
// [branch 008] With safe assertions, this becomes safe
__attribute__((transaction_safe))
static void item_unlink_q(item *it);

/*
 * We only reposition items in the LRU queue if they haven't been repositioned
 * in this many seconds. That saves us from churning on frequently-accessed
 * items.
 */
#define ITEM_UPDATE_INTERVAL 60

#define LARGEST_ID POWER_LARGEST
typedef struct {
    uint64_t evicted;
    uint64_t evicted_nonzero;
    rel_time_t evicted_time;
    uint64_t reclaimed;
    uint64_t outofmemory;
    uint64_t tailrepairs;
    uint64_t expired_unfetched;
    uint64_t evicted_unfetched;
} itemstats_t;

static item *heads[LARGEST_ID];
static item *tails[LARGEST_ID];
static itemstats_t itemstats[LARGEST_ID];
static unsigned int sizes[LARGEST_ID];

void item_stats_reset(void) {
    // [branch 002] Replaced cache_lock critical section lock with relaxed
    //              transaction
    // [branch 005] This transaction can be atomic
    __transaction_atomic {
    memset(itemstats, 0, sizeof(itemstats));
    }
}


/* Get the next CAS id for a new item. */
uint64_t get_cas_id(void) {
    static uint64_t cas_id = 0;
    return ++cas_id;
}

/* Enable this for reference-count debugging. */
#if 0
# define DEBUG_REFCNT(it,op) \
                fprintf(stderr, "item %x refcnt(%c) %d %c%c%c\n", \
                        it, op, it->refcount, \
                        (it->it_flags & ITEM_LINKED) ? 'L' : ' ', \
                        (it->it_flags & ITEM_SLABBED) ? 'S' : ' ')
#else
# define DEBUG_REFCNT(it,op) while(0)
#endif

/**
 * Generates the variable-sized part of the header for an object.
 *
 * key     - The key
 * nkey    - The length of the key
 * flags   - key flags
 * nbytes  - Number of bytes to hold value and addition CRLF terminator
 * suffix  - Buffer for the "VALUE" line suffix (flags, size).
 * nsuffix - The length of the suffix is stored here.
 *
 * Returns the total size of the header.
 */
// [branch 004b] This function is called from a relaxed transaction
// [branch 011b] This becomes safe with safe snprintf
__attribute__((transaction_safe))
static size_t item_make_header(const uint8_t nkey, const int flags, const int nbytes,
                     char *suffix, uint8_t *nsuffix) {
    /* suffix is defined at 40 chars elsewhere.. */
    // [branch 011b] The trick here is that suffix is a stack-local var at
    //               every call site.  Consequently, we can just forward to a
    //               pure snprintf variant
    *nsuffix = (uint8_t) tm_snprintf_d_d(suffix, 40, " %d %d\r\n", flags, nbytes - 2);
    return sizeof(item) + nkey + *nsuffix + nbytes;
}

/*@null@*/
item *do_item_alloc(char *key, const size_t nkey, const int flags,
                    const rel_time_t exptime, const int nbytes,
                    const uint32_t cur_hv) {
    uint8_t nsuffix;
    item *it = NULL;
    char suffix[40];
    size_t ntotal = item_make_header(nkey + 1, flags, nbytes, suffix, &nsuffix);
    if (settings.use_cas) {
        ntotal += sizeof(uint64_t);
    }

    unsigned int id = slabs_clsid(ntotal);
    if (id == 0)
        return 0;

    // [branch 002] Replaced cache_lock critical section lock with relaxed
    //              transaction
    // [branch 012] With oncommit, this becomes atomic
    __transaction_atomic {
    /* do a quick check if we have any expired items in the tail.. */
    int tries = 5;
    int tried_alloc = 0;
    item *search;
    // [branch 003b] Removed hold_lock, since we don't grab an item_lock
    rel_time_t oldest_live = settings.oldest_live;

    search = tails[id];
    /* We walk up *only* for locked items. Never searching for expired.
     * Waste of CPU for almost all deployments */
    for (; tries > 0 && search != NULL; tries--, search=search->prev) {
        uint32_t hv = hash(ITEM_key(search), search->nkey, 0);
        /* Attempt to hash item lock the "search" item. If locked, no
         * other callers can incr the refcount
         */
        /* FIXME: I think we need to mask the hv here for comparison? */
        // [branch 003b] We used to try to get an item lock, and /continue/
        //               if the acquire failed.  Now there is no lock, so we
        //               just assume we have access to the datum and keep
        //               going.
        /* Now see if the item is refcount locked */
        // [branch 007] use new tm refcounts
        if (tm_refcount_incr(&search->tm_refcount) != 2) {
            tm_refcount_decr(&search->tm_refcount);
            /* Old rare bug could cause a refcount leak. We haven't seen
             * it in years, but we leave this code in to prevent failures
             * just in case */
            // [branch 006] Use an expression to access time
            if (search->time + TAIL_REPAIR_TIME < __transaction_atomic(tm_current_time)) {
                itemstats[id].tailrepairs++;
                // [branch 007] use new tm refcounts
                search->tm_refcount = 1;
                do_item_unlink_nolock(search, hv);
            }
            // [branch 003b] No item_lock to release
            continue;
        }

        /* Expired or flushed */
        // [branch 006] Use an expression to access time
        if ((search->exptime != 0 && search->exptime < __transaction_atomic(tm_current_time))
            || (search->time <= oldest_live && oldest_live <= __transaction_atomic(tm_current_time))) {
            itemstats[id].reclaimed++;
            if ((search->it_flags & ITEM_FETCHED) == 0) {
                itemstats[id].expired_unfetched++;
            }
            it = search;
            slabs_adjust_mem_requested(it->slabs_clsid, ITEM_ntotal(it), ntotal);
            do_item_unlink_nolock(it, hv);
            /* Initialize the item block: */
            it->slabs_clsid = 0;
        } else if ((it = slabs_alloc(ntotal, id)) == NULL) {
            tried_alloc = 1;
            if (settings.evict_to_free == 0) {
                itemstats[id].outofmemory++;
            } else {
                itemstats[id].evicted++;
                // [branch 006] Use an expression to access time
                itemstats[id].evicted_time = __transaction_atomic(tm_current_time) - search->time;
                if (search->exptime != 0)
                    itemstats[id].evicted_nonzero++;
                if ((search->it_flags & ITEM_FETCHED) == 0) {
                    itemstats[id].evicted_unfetched++;
                }
                it = search;
                slabs_adjust_mem_requested(it->slabs_clsid, ITEM_ntotal(it), ntotal);
                do_item_unlink_nolock(it, hv);
                /* Initialize the item block: */
                it->slabs_clsid = 0;

                /* If we've just evicted an item, and the automover is set to
                 * angry bird mode, attempt to rip memory into this slab class.
                 * TODO: Move valid object detection into a function, and on a
                 * "successful" memory pull, look behind and see if the next alloc
                 * would be an eviction. Then kick off the slab mover before the
                 * eviction happens.
                 */
                if (settings.slab_automove == 2)
                    slabs_reassign(-1, id);
            }
        }

        // [branch 007] use new tm refcounts
        tm_refcount_decr(&search->tm_refcount);
        /* If hash values were equal, we don't grab a second lock */
        // [branch 003b] No item_lock to release
        break;
    }

    if (!tried_alloc && (tries == 0 || search == NULL))
        it = slabs_alloc(ntotal, id);

    if (it == NULL) {
        itemstats[id].outofmemory++;
        return NULL;
    }

    // [branch 008] Switch to safe assertions
    tm_assert(it->slabs_clsid == 0);
    tm_assert(it != heads[id]);

    /* Item initialization can happen outside of the lock; the item's already
     * been removed from the slab LRU.
     */
    // [branch 007] use new tm refcounts
    it->tm_refcount = 1;     /* the caller will have a reference */
    }
    it->next = it->prev = it->h_next = 0;
    it->slabs_clsid = id;

    DEBUG_REFCNT(it, '*');
    it->it_flags = settings.use_cas ? ITEM_CAS : 0;
    it->nkey = nkey;
    it->nbytes = nbytes;
    // [branch 009b] Use safe memcpy
    tm_memcpy(ITEM_key(it), key, nkey);
    it->exptime = exptime;
    // [branch 009b] Use safe memcpy
    tm_memcpy(ITEM_suffix(it), suffix, (size_t)nsuffix);
    it->nsuffix = nsuffix;
    return it;
}

void item_free(item *it) {
    size_t ntotal = ITEM_ntotal(it);
    unsigned int clsid;
    // [branch 008] switch to safe assertions
    tm_assert((it->it_flags & ITEM_LINKED) == 0);
    tm_assert(it != heads[it->slabs_clsid]);
    tm_assert(it != tails[it->slabs_clsid]);
    // [branch 007] use an expression to read refcounts
    tm_assert(__transaction_atomic(it->tm_refcount) == 0);

    /* so slab size changer can tell later if item is already free or not */
    clsid = it->slabs_clsid;
    it->slabs_clsid = 0;
    DEBUG_REFCNT(it, 'F');
    slabs_free(it, ntotal, clsid);
}

/**
 * Returns true if an item will fit in the cache (its size does not exceed
 * the maximum for a cache entry.)
 */
bool item_size_ok(const size_t nkey, const int flags, const int nbytes) {
    char prefix[40];
    uint8_t nsuffix;

    size_t ntotal = item_make_header(nkey + 1, flags, nbytes,
                                     prefix, &nsuffix);
    if (settings.use_cas) {
        ntotal += sizeof(uint64_t);
    }

    return slabs_clsid(ntotal) != 0;
}

static void item_link_q(item *it) { /* item is the new head */
    item **head, **tail;
    // [branch 008] switch to safe assertions
    tm_assert(it->slabs_clsid < LARGEST_ID);
    tm_assert((it->it_flags & ITEM_SLABBED) == 0);

    head = &heads[it->slabs_clsid];
    tail = &tails[it->slabs_clsid];
    // [branch 008] switch to safe assertions
    tm_assert(it != *head);
    tm_assert((*head && *tail) || (*head == 0 && *tail == 0));
    it->prev = 0;
    it->next = *head;
    if (it->next) it->next->prev = it;
    *head = it;
    if (*tail == 0) *tail = it;
    sizes[it->slabs_clsid]++;
    return;
}

static void item_unlink_q(item *it) {
    item **head, **tail;
    // [branch 008] switch to safe assertions
    tm_assert(it->slabs_clsid < LARGEST_ID);
    head = &heads[it->slabs_clsid];
    tail = &tails[it->slabs_clsid];

    if (*head == it) {
        // [branch 008] switch to safe assertions
        tm_assert(it->prev == 0);
        *head = it->next;
    }
    if (*tail == it) {
        // [branch 008] switch to safe assertions
        tm_assert(it->next == 0);
        *tail = it->prev;
    }
    // [branch 008] switch to safe assertions
    tm_assert(it->next != it);
    tm_assert(it->prev != it);

    if (it->next) it->next->prev = it->prev;
    if (it->prev) it->prev->next = it->next;
    sizes[it->slabs_clsid]--;
    return;
}

int do_item_link(item *it, const uint32_t hv) {
    MEMCACHED_ITEM_LINK(ITEM_key(it), it->nkey, it->nbytes);
    // [branch 008b] Switch to safe assertions
    tm_assert((it->it_flags & (ITEM_LINKED|ITEM_SLABBED)) == 0);
    // [branch 002] Replaced cache_lock critical section lock with relaxed
    //              transaction
    // [branch 012] oncommit makes assoc_insert safe, and this atomic
    __transaction_atomic {
    it->it_flags |= ITEM_LINKED;
    // [branch 006] Use an expression to access time
    it->time = __transaction_atomic(tm_current_time);

    // [branch 002] elide STATS_LOCK since we're in a relaxed transaction
    stats.curr_bytes += ITEM_ntotal(it);
    stats.curr_items += 1;
    stats.total_items += 1;

    /* Allocate a new CAS ID on link. */
    ITEM_set_cas(it, (settings.use_cas) ? get_cas_id() : 0);
    assoc_insert(it, hv);
    item_link_q(it);
    // [branch 007] use new tm refcounts
    tm_refcount_incr(&it->tm_refcount);
    }

    return 1;
}

void do_item_unlink(item *it, const uint32_t hv) {
    MEMCACHED_ITEM_UNLINK(ITEM_key(it), it->nkey, it->nbytes);
    // [branch 002] Replaced cache_lock critical section lock with relaxed
    //              transaction
    // [branch 009] With safe memcmp, this transaction becomes atomic
    __transaction_atomic {
    if ((it->it_flags & ITEM_LINKED) != 0) {
        it->it_flags &= ~ITEM_LINKED;
        // [branch 002] elide STATS_LOCK since we're in a relaxed transaction
        stats.curr_bytes -= ITEM_ntotal(it);
        stats.curr_items -= 1;
        assoc_delete(ITEM_key(it), it->nkey, hv);
        item_unlink_q(it);
        do_item_remove(it);
    }
    }
}

/* FIXME: Is it necessary to keep this copy/pasted code? */
void do_item_unlink_nolock(item *it, const uint32_t hv) {
    MEMCACHED_ITEM_UNLINK(ITEM_key(it), it->nkey, it->nbytes);
    if ((it->it_flags & ITEM_LINKED) != 0) {
        it->it_flags &= ~ITEM_LINKED;
        // [branch 002] Replaced STATS_LOCK with relaxed transaction
        // [branch 005] This transaction can be atomic
        __transaction_atomic {
        stats.curr_bytes -= ITEM_ntotal(it);
        stats.curr_items -= 1;
        }
        assoc_delete(ITEM_key(it), it->nkey, hv);
        item_unlink_q(it);
        do_item_remove(it);
    }
}

void do_item_remove(item *it) {
    MEMCACHED_ITEM_REMOVE(ITEM_key(it), it->nkey, it->nbytes);
    // [branch 008] switch to safe assertions
    tm_assert((it->it_flags & ITEM_SLABBED) == 0);

    // [branch 007] use new tm refcounts
    if (tm_refcount_decr(&it->tm_refcount) == 0) {
        item_free(it);
    }
}

void do_item_update(item *it) {
    MEMCACHED_ITEM_UPDATE(ITEM_key(it), it->nkey, it->nbytes);
    // [branch 006] Use an expression to access time
    if (it->time < __transaction_atomic(tm_current_time) - ITEM_UPDATE_INTERVAL) {
        // [branch 008b] Switch to safe assertions
        tm_assert((it->it_flags & ITEM_SLABBED) == 0);

        // [branch 002] Replaced cache_lock critical section lock with
        //              relaxed transaction
        // [branch 008] With safe asserts, this becomes atomic
        __transaction_atomic {
        if ((it->it_flags & ITEM_LINKED) != 0) {
            item_unlink_q(it);
            // [branch 006] Use an expression to access time
            it->time = __transaction_atomic(tm_current_time);
            item_link_q(it);
        }
        }
    }
}

int do_item_replace(item *it, item *new_it, const uint32_t hv) {
    MEMCACHED_ITEM_REPLACE(ITEM_key(it), it->nkey, it->nbytes,
                           ITEM_key(new_it), new_it->nkey, new_it->nbytes);
    // [branch 008b] Switch to safe assertions
    tm_assert((it->it_flags & ITEM_SLABBED) == 0);

    do_item_unlink(it, hv);
    return do_item_link(new_it, hv);
}

/*@null@*/
char *do_item_cachedump(const unsigned int slabs_clsid, const unsigned int limit, unsigned int *bytes) {
    unsigned int memlimit = 2 * 1024 * 1024;   /* 2MB max response size */
    char *buffer;
    unsigned int bufcurr;
    item *it;
    unsigned int len;
    unsigned int shown = 0;
    char key_temp[KEY_MAX_LENGTH + 1];
    char temp[512];

    it = heads[slabs_clsid];

    buffer = malloc((size_t)memlimit);
    if (buffer == 0) return NULL;
    bufcurr = 0;

    while (it != NULL && (limit == 0 || shown < limit)) {
        // [branch 008] switch to safe assertions
        tm_assert(it->nkey <= KEY_MAX_LENGTH);
        /* Copy the key since it may not be null-terminated in the struct */
        // [branch 009] Switch to safe strncpy
        // [branch 011] Switch to strncpy_to_local so that we can marshall
        //              the result to a tm_snprintf variant
        tm_strncpy_to_local(key_temp, ITEM_key(it), it->nkey);
        key_temp[it->nkey] = 0x00; /* terminate */
        // [branch 011] Write the marshalled string into temp string.  Since
        //              all is local, we can use a pure wrapper on snprintf
        len = tm_snprintf_s_d_lu(temp, sizeof(temp), "ITEM %s [%d b; %lu s]\r\n",
                                 key_temp, it->nbytes - 2,
                                 (unsigned long)it->exptime + process_started);
        if (bufcurr + len + 6 > memlimit)  /* 6 is END\r\n\0 */
            break;
        // [branch 009] Replace memcpy with safe alternative
        tm_memcpy(buffer + bufcurr, temp, len);
        bufcurr += len;
        shown++;
        it = it->next;
    }

    // [branch 009] Replace memcpy with safe alternative
    tm_memcpy(buffer + bufcurr, "END\r\n", 6);
    bufcurr += 5;

    *bytes = bufcurr;
    return buffer;
}

void item_stats_evictions(uint64_t *evicted) {
    int i;
    // [branch 002] Replaced cache_lock critical section lock with relaxed
    //              transaction
    // [branch 005] This transaction can be atomic
    __transaction_atomic {
    for (i = 0; i < LARGEST_ID; i++) {
        evicted[i] = itemstats[i].evicted;
    }
    }
}

void do_item_stats_totals(ADD_STAT add_stats, void *c) {
    itemstats_t totals;
    memset(&totals, 0, sizeof(itemstats_t));
    int i;
    for (i = 0; i < LARGEST_ID; i++) {
        totals.expired_unfetched += itemstats[i].expired_unfetched;
        totals.evicted_unfetched += itemstats[i].evicted_unfetched;
        totals.evicted += itemstats[i].evicted;
        totals.reclaimed += itemstats[i].reclaimed;
    }
    // [branch 011] Use safe append_stat calls
    APPEND_STAT_LLU("expired_unfetched", "%llu",
                (unsigned long long)totals.expired_unfetched);
    APPEND_STAT_LLU("evicted_unfetched", "%llu",
                (unsigned long long)totals.evicted_unfetched);
    APPEND_STAT_LLU("evictions", "%llu",
                (unsigned long long)totals.evicted);
    APPEND_STAT_LLU("reclaimed", "%llu",
                (unsigned long long)totals.reclaimed);
}

void do_item_stats(ADD_STAT add_stats, void *c) {
    int i;
    for (i = 0; i < LARGEST_ID; i++) {
        if (tails[i] != NULL) {
            const char *fmt = "items:%d:%s";
            char key_str[STAT_KEY_LEN];
            char val_str[STAT_VAL_LEN];
            int klen = 0, vlen = 0;
            if (tails[i] == NULL) {
                /* We removed all of the items in this slab class */
                continue;
            }
            // [branch 011] Use safe stat function calls here
            APPEND_NUM_FMT_STAT_U(fmt, i, "number", "%u", sizes[i]);
            // [branch 006] Use an expression to access time
            APPEND_NUM_FMT_STAT_U(fmt, i, "age", "%u", __transaction_atomic(tm_current_time) - tails[i]->time);
            APPEND_NUM_FMT_STAT_U(fmt, i, "evicted",
                                "%llu", (unsigned long long)itemstats[i].evicted);
            APPEND_NUM_FMT_STAT_LLU(fmt, i, "evicted_nonzero",
                                "%llu", (unsigned long long)itemstats[i].evicted_nonzero);
            APPEND_NUM_FMT_STAT_U(fmt, i, "evicted_time",
                                "%u", itemstats[i].evicted_time);
            APPEND_NUM_FMT_STAT_LLU(fmt, i, "outofmemory",
                                "%llu", (unsigned long long)itemstats[i].outofmemory);
            APPEND_NUM_FMT_STAT_LLU(fmt, i, "tailrepairs",
                                "%llu", (unsigned long long)itemstats[i].tailrepairs);
            APPEND_NUM_FMT_STAT_LLU(fmt, i, "reclaimed",
                                "%llu", (unsigned long long)itemstats[i].reclaimed);
            APPEND_NUM_FMT_STAT_LLU(fmt, i, "expired_unfetched",
                                "%llu", (unsigned long long)itemstats[i].expired_unfetched);
            APPEND_NUM_FMT_STAT_LLU(fmt, i, "evicted_unfetched",
                                "%llu", (unsigned long long)itemstats[i].evicted_unfetched);
        }
    }

    /* getting here means both ascii and binary terminators fit */
    add_stats(NULL, 0, NULL, 0, c);
}

/** dumps out a list of objects of each size, with granularity of 32 bytes */
/*@null@*/
void do_item_stats_sizes(ADD_STAT add_stats, void *c) {

    /* max 1MB object, divided into 32 bytes size buckets */
    const int num_buckets = 32768;
    unsigned int *histogram = calloc(num_buckets, sizeof(int));

    if (histogram != NULL) {
        int i;

        /* build the histogram */
        for (i = 0; i < LARGEST_ID; i++) {
            item *iter = heads[i];
            while (iter) {
                int ntotal = ITEM_ntotal(iter);
                int bucket = ntotal / 32;
                if ((ntotal % 32) != 0) bucket++;
                if (bucket < num_buckets) histogram[bucket]++;
                iter = iter->next;
            }
        }

        /* write the buffer */
        for (i = 0; i < num_buckets; i++) {
            if (histogram[i] != 0) {
                char key[8];
                // [branch 011] We can use our safe snprintf on this
                tm_snprintf_d(key, sizeof(key), "%d", i * 32);
                // [branch 011] Use safe stats
                APPEND_STAT_U(key, "%u", histogram[i]);
            }
        }
        free(histogram);
    }
    add_stats(NULL, 0, NULL, 0, c);
}

// [branch 012b] oncommit wrapper for each of 5 fprintfs in do_item_get
//
// WARNING: We probably need to marshall the string parameters to fprintf[12]
// into heap variables, and we aren't right now.  If verbose is off, it
// doesn't matter, but if verbose is on, we might print garbage.
static void dig_fprintf1(void *param)
{
    fprintf(stderr, "> NOT FOUND %s", (char*)param);
}
static void dig_fprintf2(void *param)
{
    fprintf(stderr, "> FOUND KEY %s", (char*)param);
}
static void dig_fprintf3(void *param)
{
    fprintf(stderr, " -nuked by flush");
}
static void dig_fprintf4(void *param)
{
    fprintf(stderr, " -nuked by expire");
}
static void dig_fprintf5(void *param)
{
    fprintf(stderr, "\n");
}

/** wrapper around assoc_find which does the lazy expiration logic */
item *do_item_get(const char *key, const size_t nkey, const uint32_t hv) {
    //mutex_lock(&cache_lock);
    item *it = assoc_find(key, nkey, hv);
    if (it != NULL) {
        // [branch 007] use new tm refcounts
        tm_refcount_incr(&it->tm_refcount);
        /* Optimization for slab reassignment. prevents popular items from
         * jamming in busy wait. Can only do this here to satisfy lock order
         * of item_lock, cache_lock, slabs_lock. */
        // [branch 006] Use a transaction expression to check the (renamed)
        //              rebalance signal
        if (__transaction_atomic(tm_slab_rebalance_signal) &&
            ((void *)it >= slab_rebal.slab_start && (void *)it < slab_rebal.slab_end)) {
            do_item_unlink_nolock(it, hv);
            do_item_remove(it);
            it = NULL;
        }
    }
    //mutex_unlock(&cache_lock);
    int was_found = 0;

    if (settings.verbose > 2) {
        if (it == NULL) {
            // [branch 012b] Switch to oncommit handler
            registerOnCommitHandler(dig_fprintf1, (void*)key);
        } else {
            // [branch 012b] Switch to oncommit handler
            registerOnCommitHandler(dig_fprintf2, ITEM_key(it));
            was_found++;
        }
    }

    if (it != NULL) {
        // [branch 006] Use an expression to access time
        if (settings.oldest_live != 0 && settings.oldest_live <= __transaction_atomic(tm_current_time) &&
            it->time <= settings.oldest_live) {
            do_item_unlink(it, hv);
            do_item_remove(it);
            it = NULL;
            if (was_found) {
                // [branch 012b] Switch to oncommit handler
                registerOnCommitHandler(dig_fprintf3, NULL);
            }
        }
        // [branch 006] Use an expression to access time
        else if (it->exptime != 0 && it->exptime <= __transaction_atomic(tm_current_time)) {
            do_item_unlink(it, hv);
            do_item_remove(it);
            it = NULL;
            if (was_found) {
                // [branch 012b] Switch to oncommit handler
                registerOnCommitHandler(dig_fprintf4, NULL);
            }
        } else {
            it->it_flags |= ITEM_FETCHED;
            DEBUG_REFCNT(it, '+');
        }
    }

    if (settings.verbose > 2)
        // [branch 012b] Switch to oncommit handler
        registerOnCommitHandler(dig_fprintf5, NULL);

    return it;
}

item *do_item_touch(const char *key, size_t nkey, uint32_t exptime,
                    const uint32_t hv) {
    item *it = do_item_get(key, nkey, hv);
    if (it != NULL) {
        it->exptime = exptime;
    }
    return it;
}

/* expires items that are more recent than the oldest_live setting. */
void do_item_flush_expired(void) {
    int i;
    item *iter, *next;
    if (settings.oldest_live == 0)
        return;
    for (i = 0; i < LARGEST_ID; i++) {
        /* The LRU is sorted in decreasing time order, and an item's timestamp
         * is never newer than its last access time, so we only need to walk
         * back until we hit an item older than the oldest_live time.
         * The oldest_live checking will auto-expire the remaining items.
         */
        for (iter = heads[i]; iter != NULL; iter = next) {
            if (iter->time >= settings.oldest_live) {
                next = iter->next;
                if ((iter->it_flags & ITEM_SLABBED) == 0) {
                    do_item_unlink_nolock(iter, hash(ITEM_key(iter), iter->nkey, 0));
                }
            } else {
                /* We've hit the first old item. Continue to the next queue. */
                break;
            }
        }
    }
}
