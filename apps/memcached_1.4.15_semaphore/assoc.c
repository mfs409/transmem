/* -*- Mode: C; tab-width: 4; c-basic-offset: 4; indent-tabs-mode: nil -*- */
/*
 * Hash table
 *
 * The hash function used here is by Bob Jenkins, 1996:
 *    <http://burtleburtle.net/bob/hash/doobs.html>
 *       "By Bob Jenkins, 1996.  bob_jenkins@burtleburtle.net.
 *       You may use this code any way you wish, private, educational,
 *       or commercial.  It's free."
 *
 * The rest of the file is licensed under the BSD license.  See LICENSE.
 */

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
#include <assert.h>
#include <pthread.h>

#include <semaphore.h>
// [branch 001] switched from pthread_cod_t maintenance cond to sem_t mx_sem
sem_t mx_sem;

// [branch 001] sem_t doesn't have a static initializer, so we have this
// function to help with initialization
static void early_sem_init()
{
    sem_init(&mx_sem, 0, 0);
}

typedef  unsigned long  int  ub4;   /* unsigned 4-byte quantities */
typedef  unsigned       char ub1;   /* unsigned 1-byte quantities */

/* how many powers of 2's worth of buckets we use */
unsigned int hashpower = HASHPOWER_DEFAULT;

#define hashsize(n) ((ub4)1<<(n))
#define hashmask(n) (hashsize(n)-1)

/* Main hash table. This is where we look except during expansion. */
static item** primary_hashtable = 0;

/*
 * Previous hash table. During expansion, we look here for keys that haven't
 * been moved over to the primary yet.
 */
static item** old_hashtable = 0;

/* Number of items in the hash table. */
static unsigned int hash_items = 0;

/* Flag: Are we in the middle of expanding now? */
static bool expanding = false;
static bool started_expanding = false;

/*
 * During expansion we migrate values with bucket granularity; this is how
 * far we've gotten so far. Ranges from 0 .. hashsize(hashpower - 1) - 1.
 */
static unsigned int expand_bucket = 0;

void assoc_init(const int hashtable_init) {
    // [branch 001] Initialize the semaphore.
    //
    // NB: we checked, and this is called early enough that the semaphore
    // will be initialized before worker threads are created.
    early_sem_init();

    if (hashtable_init) {
        hashpower = hashtable_init;
    }
    primary_hashtable = calloc(hashsize(hashpower), sizeof(void *));
    if (! primary_hashtable) {
        fprintf(stderr, "Failed to init hashtable.\n");
        exit(EXIT_FAILURE);
    }
    // [branch 002] Replaced STATS_LOCK with relaxed transaction
    // [branch 005] This transaction can be atomic
    __transaction_atomic {
    stats.hash_power_level = hashpower;
    stats.hash_bytes = hashsize(hashpower) * sizeof(void *);
    }
}

item *assoc_find(const char *key, const size_t nkey, const uint32_t hv) {
    item *it;
    unsigned int oldbucket;

    if (expanding &&
        (oldbucket = (hv & hashmask(hashpower - 1))) >= expand_bucket)
    {
        it = old_hashtable[oldbucket];
    } else {
        it = primary_hashtable[hv & hashmask(hashpower)];
    }

    item *ret = NULL;
    int depth = 0;
    while (it) {
        // [branch 009b] Switch to safe memcmp
        if ((nkey == it->nkey) && (tm_memcmp(key, ITEM_key(it), nkey) == 0)) {
            ret = it;
            break;
        }
        it = it->h_next;
        ++depth;
    }
    MEMCACHED_ASSOC_FIND(key, nkey, depth);
    return ret;
}

/* returns the address of the item pointer before the key.  if *item == 0,
   the item wasn't found */

// [branch 004] This function is called from a relaxed transaction
// [branch 009] With safe memcmp, this function becomes safe
__attribute__((transaction_safe))
static item** _hashitem_before (const char *key, const size_t nkey, const uint32_t hv) {
    item **pos;
    unsigned int oldbucket;

    if (expanding &&
        (oldbucket = (hv & hashmask(hashpower - 1))) >= expand_bucket)
    {
        pos = &old_hashtable[oldbucket];
    } else {
        pos = &primary_hashtable[hv & hashmask(hashpower)];
    }

    // [branch 009] Switch to safe memcmp
    while (*pos && ((nkey != (*pos)->nkey) || tm_memcmp(key, ITEM_key(*pos), nkey))) {
        pos = &(*pos)->h_next;
    }
    return pos;
}

// [branch 012] Moved call out so we can use a commit handler
static void ae_fprintf1(void *param)
{
    fprintf(stderr, "Hash table expansion starting\n");
}

/* grows the hashtable to the next power of 2. */
// [branch 004] This function is called from a relaxed transaction
// [branch 012] With oncommit, this becomes safe
__attribute__((transaction_safe))
static void assoc_expand(void) {
    old_hashtable = primary_hashtable;

    primary_hashtable = calloc(hashsize(hashpower + 1), sizeof(void *));
    if (primary_hashtable) {
        if (settings.verbose > 1)
            // [branch 012] Replace fprintf with oncommit
            registerOnCommitHandler(ae_fprintf1, NULL);
        hashpower++;
        expanding = true;
        expand_bucket = 0;
        // [branch 002] Replaced STATS_LOCK with relaxed transaction
        // [branch 005] This transaction can be atomic
        __transaction_atomic {
        stats.hash_power_level = hashpower;
        stats.hash_bytes += hashsize(hashpower) * sizeof(void *);
        stats.hash_is_expanding = 1;
        }
    } else {
        primary_hashtable = old_hashtable;
        /* Bad news, but we can keep running. */
    }
}

// [branch 012] Move sempost to oncommit handler
static void ase_sempost1(void *param)
{
    sem_post(&mx_sem);
}

// [branch 004] This function is called from a relaxed transaction
// [branch 012] With oncommit, this becomes safe
__attribute__((transaction_safe))
static void assoc_start_expand(void) {
    if (started_expanding)
        return;
    started_expanding = true;
    // [branch 001] rather than signal on a condvar, we post on a semaphore
    // [branch 012] move sempost to oncommit
    registerOnCommitHandler(ase_sempost1, NULL);
}

/* Note: this isn't an assoc_update.  The key must not already exist to call this */
int assoc_insert(item *it, const uint32_t hv) {
    unsigned int oldbucket;

//    assert(assoc_find(ITEM_key(it), it->nkey) == 0);  /* shouldn't have duplicately named things defined */

    if (expanding &&
        (oldbucket = (hv & hashmask(hashpower - 1))) >= expand_bucket)
    {
        it->h_next = old_hashtable[oldbucket];
        old_hashtable[oldbucket] = it;
    } else {
        it->h_next = primary_hashtable[hv & hashmask(hashpower)];
        primary_hashtable[hv & hashmask(hashpower)] = it;
    }

    hash_items++;
    if (! expanding && hash_items > (hashsize(hashpower) * 3) / 2) {
        assoc_start_expand();
    }

    MEMCACHED_ASSOC_INSERT(ITEM_key(it), it->nkey, hash_items);
    return 1;
}

void assoc_delete(const char *key, const size_t nkey, const uint32_t hv) {
    item **before = _hashitem_before(key, nkey, hv);

    if (*before) {
        item *nxt;
        hash_items--;
        /* The DTrace probe cannot be triggered as the last instruction
         * due to possible tail-optimization by the compiler
         */
        MEMCACHED_ASSOC_DELETE(key, nkey, hash_items);
        nxt = (*before)->h_next;
        (*before)->h_next = 0;   /* probably pointless, but whatever. */
        *before = nxt;
        return;
    }
    /* Note:  we never actually get here.  the callers don't delete things
       they can't find. */
    // [branch 008] switch to safe assertions
    tm_assert(*before != 0);
}

// [branch 006] Replace volatile variable with "transactional" variable
static int tm_do_run_maintenance_thread = 1;

#define DEFAULT_HASH_BULK_MOVE 1
int hash_bulk_move = DEFAULT_HASH_BULK_MOVE;

// [branch 012] wrap fprintf so we can do it in oncommit
static void amt_fprintf1(void *param)
{
    fprintf(stderr, "Hash table expansion done\n");
}

static void *assoc_maintenance_thread(void *arg) {

    // [branch 006] use a transaction expression to access a
    //              formerly-volatile variable
    while (__transaction_atomic(tm_do_run_maintenance_thread)) {
        int ii = 0;

        /* Lock the cache, and bulk move multiple buckets to the new
         * hash table. */
        // [branch 003b] Can elide lock/unlock of item_global_lock
        // [branch 002] Replaced cache_lock critical section lock with relaxed
        //              transaction
        // [branch 012] With oncommit, this becomes atomic
        __transaction_atomic {

        for (ii = 0; ii < hash_bulk_move && expanding; ++ii) {
            item *it, *next;
            int bucket;

            for (it = old_hashtable[expand_bucket]; NULL != it; it = next) {
                next = it->h_next;

                bucket = hash(ITEM_key(it), it->nkey, 0) & hashmask(hashpower);
                it->h_next = primary_hashtable[bucket];
                primary_hashtable[bucket] = it;
            }

            old_hashtable[expand_bucket] = NULL;

            expand_bucket++;
            if (expand_bucket == hashsize(hashpower - 1)) {
                expanding = false;
                free(old_hashtable);
                // [branch 002] elide STATS_LOCK since we're in a relaxed
                //              transaction
                stats.hash_bytes -= hashsize(hashpower - 1) * sizeof(void *);
                stats.hash_is_expanding = 0;
                if (settings.verbose > 1)
                    // [branch 012] Move fprintf to oncommit
                    registerOnCommitHandler(amt_fprintf1, NULL);
            }
        }

        }

        if (!expanding) {
            /* finished expanding. tell all threads to use fine-grained locks */
            switch_item_lock_type(ITEM_LOCK_GRANULAR);
            slabs_rebalancer_resume();
            /* We are done expanding.. just wait for next invocation */
            // [branch 002] Replaced cache_lock critical section lock with
            //              relaxed transaction
            // [branch 005] This transaction can be atomic
            __transaction_atomic {
            started_expanding = false;
            // [branch 001] instead of waiting on a condvar, then unlocking,
            //              we can unlock, then wait on a semaphore
            }
            sem_wait(&mx_sem);
            /* Before doing anything, tell threads to use a global lock */
            slabs_rebalancer_pause();
            switch_item_lock_type(ITEM_LOCK_GLOBAL);
            // [branch 002] Replaced cache_lock critical section lock with
            //              relaxed transaction
            // [branch 012] This becomes atomic once assoc_expand fprintf is
            //              oncommit
            __transaction_atomic {
            assoc_expand();
            }
        }
    }
    return NULL;
}

static pthread_t maintenance_tid;

int start_assoc_maintenance_thread() {
    int ret;
    char *env = getenv("MEMCACHED_HASH_BULK_MOVE");
    if (env != NULL) {
        hash_bulk_move = atoi(env);
        if (hash_bulk_move == 0) {
            hash_bulk_move = DEFAULT_HASH_BULK_MOVE;
        }
    }
    if ((ret = pthread_create(&maintenance_tid, NULL,
                              assoc_maintenance_thread, NULL)) != 0) {
        fprintf(stderr, "Can't create thread: %s\n", strerror(ret));
        return -1;
    }
    return 0;
}

void stop_assoc_maintenance_thread() {
    // [branch 002] Replaced cache_lock critical section lock with relaxed
    //              transaction
    // [branch 006] This can now be an atomic transaction, since the variable
    //              it accesses is no longer volatile
    __transaction_atomic {
    tm_do_run_maintenance_thread = 0;
    // [branch 001] instead of signalling a condvar while holding the lock,
    //              we can release the lock and then post on the semaphore
    }
    sem_post(&mx_sem);

    /* Wait for the maintenance thread to stop */
    pthread_join(maintenance_tid, NULL);
}


