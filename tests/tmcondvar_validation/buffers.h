#pragma once

#include <semaphore.h>
#include <tmcondvar.h>
#include <cstdlib>
#include <cmath>

/// A basic bounded buffer object, that we can employ in order to evaluate
/// different forms of synchronization
///
/// Note that there are better (lower contention) ways to implement a
/// producer-consumer buffer.  This is intended only to serve as a test for
/// making sure the various condition synchronization mechanisms work.
struct buffer_t
{
    int* buf;                   /// where the stuff goes
    int  capacity;              /// size of the buffer
    int  size;                  /// number of elements currently in it
    int  fill;                  /// position for next put
    int  use;                   /// position for next get

    /// place /value/ into next slot in the buffer; assumes that a slot is
    /// available
    void put_unchecked(int value) {
        buf[fill] = value;
        fill = (fill + 1) % capacity;
        size++;
    }

    /// get entry from buffer; assumes buffer is not empty
    int get_unchecked() {
        int tmp = buf[use];
        use = (use + 1) % capacity;
        size--;
        return tmp;
    }

    /// construct a new buffer of size /cap/, and populate it according to
    /// /preload_factor/
    buffer_t(int cap, float preload_factor)
        : capacity(cap), size(0), fill(0), use(0)
    {
        buf = (int*) malloc(sizeof(int)*capacity);
        if (preload_factor <= 0.0)
            return;
        if (preload_factor > 1.0)
            preload_factor = 1.0;

        for (int i = 0; i < floor(capacity*preload_factor); ++i) {
            buf[fill++] = ~0;
            size++;
        }
    }

    /// free memory on destruction
    ~buffer_t() {
        free(buf);
    }

    /// helper function to indicate there's room for another put()
    __attribute__((transaction_safe))
    bool not_full() {
        return size < capacity;
    }

    /// helper function to indicate there's something to get()
    __attribute__((transaction_safe))
    bool not_empty() {
        return size > 0;
    }
};

/// a wrapper so that our buffer_t implementations are easy to switch between
/// in the benchmark
struct synchronized_buffer_t
{
    /// safely insert an element into the buffer via pthread condvars
    virtual void put(int value) = 0;

    /// safely extract an element from the buffer via pthread condvars
    virtual int get() = 0;

    /// mandatory dtor
    virtual ~synchronized_buffer_t() { }
};

/// A buffer object synchronized via pthread_cond_t
///
/// This is the baseline
struct pthread_buffer_t : public synchronized_buffer_t
{
    pthread_cond_t  empty;  /// condvar to indicate buffer has space for put
    pthread_cond_t  full;   /// condvar to indicate buffer has content to get
    pthread_mutex_t mutex;  /// mutex for use along with condvars
    buffer_t        buffer; /// the buffer to use

    /// construct a buffer and set up condition synchronization objects
    pthread_buffer_t(int cap = 16, float preload_factor = 0.5)
        : buffer(cap, preload_factor)
    {
        pthread_cond_init(&empty, NULL);
        pthread_cond_init(&full, NULL);
        pthread_mutex_init(&mutex, NULL);
    }

    /// destruct by reclaiming pthread objects
    ~pthread_buffer_t() {
        pthread_mutex_destroy(&mutex);
        pthread_cond_destroy(&empty);
        pthread_cond_destroy(&full);
    }

    /// safely insert an element into the buffer via pthread condvars
    void put(int value) {
        pthread_mutex_lock(&mutex);
        while (!buffer.not_full())
            pthread_cond_wait(&empty, &mutex);
        buffer.put_unchecked(value);
        pthread_cond_signal(&full);
        pthread_mutex_unlock(&mutex);
    }

    /// safely extract an element from the buffer via pthread condvars
    int get() {
        pthread_mutex_lock(&mutex);
        while (!buffer.not_empty())
            pthread_cond_wait(&full, &mutex);
        int ret = buffer.get_unchecked();
        pthread_cond_signal(&empty);
        pthread_mutex_unlock(&mutex);
        return ret;
    }
};

/// A buffer object synchronized via pthread locks and semaphores
///
/// This shows the performance gap between semaphores and condvars
struct lock_sem_buffer_t : public synchronized_buffer_t
{
    sem_t           empty;   /// semaphore to indicate there's space for put
    sem_t           full;    /// semaphore to indicate there's content to get
    pthread_mutex_t mutex;   /// mutex to use along with semaphores
    buffer_t        buffer;  /// the buffer to use

    /// construct a buffer and set up mutex and semaphores
    lock_sem_buffer_t(int cap = 16, float preload_factor = 0.5)
        : buffer(cap, preload_factor)
    {
        sem_init(&empty, 0, cap);
        sem_init(&full, 0, 0);
        pthread_mutex_init(&mutex, NULL);
    }

    /// destruct by reclaiming semaphores and mutex
    ~lock_sem_buffer_t() {
        pthread_mutex_destroy(&mutex);
        sem_destroy(&empty);
        sem_destroy(&full);
    }

    /// safely insert an element via semaphores/locks
    void put(int value) {
        sem_wait(&empty);
        pthread_mutex_lock(&mutex);
        buffer.put_unchecked(value);
        pthread_mutex_unlock(&mutex);
        sem_post(&full);
    }

    /// safely extract an element via semaphores/locks
    int get() {
        sem_wait(&full);
        pthread_mutex_lock(&mutex);
        int ret = buffer.get_unchecked();
        pthread_mutex_unlock(&mutex);
        sem_post(&empty);
        return ret;
    }
};

/// A buffer object synchronized via transactions and semaphores
///
/// This shows the performance gap between locks and transactions
struct tm_sem_buffer_t : public synchronized_buffer_t
{
    sem_t    empty;          /// semaphore to indicate there's space for put
    sem_t    full;           /// semaphore to indicate there's content to get
    buffer_t buffer;         /// the buffer to use

    /// construct a buffer and set up semaphores
    tm_sem_buffer_t(int cap = 16, float preload_factor = 0.5)
        : buffer(cap, preload_factor)
    {
        sem_init(&empty, 0, cap);
        sem_init(&full, 0, 0);
    }

    /// destruct by reclaiming semaphores
    ~tm_sem_buffer_t() {
        sem_destroy(&empty);
        sem_destroy(&full);
    }

    /// safely insert an element via semaphores/transactions
    void put(int value) {
        sem_wait(&empty);
        __transaction_atomic {
            buffer.put_unchecked(value);
        }
        sem_post(&full);
    }

    /// safely extract an element via semaphores/transactions
    int get() {
        int ret;
        sem_wait(&full);
        __transaction_atomic {
            ret = buffer.get_unchecked();
        }
        sem_post(&empty);
        return ret;
    }
};

/// A buffer object synchronized via pthread locks and tmcondvars
///
/// This helps to show that our mechanism works with locks, too
struct lock_tmcondvar_buffer_t : public synchronized_buffer_t
{
    tmcondvar*      empty;  /// condvar to indicate buffer has space for put
    tmcondvar*      full;   /// condvar to indicate buffer has content to get
    pthread_mutex_t mutex;  /// mutex for protecting the buffer
    buffer_t        buffer; /// the buffer to use

    /// construct a buffer and set up lock/tmcondvars
    lock_tmcondvar_buffer_t(int cap = 16, float preload_factor = 0.5)
        : buffer(cap, preload_factor)
    {
        empty = tmcondvar_create();
        full = tmcondvar_create();
        pthread_mutex_init(&mutex, NULL);
    }

    /// destruct by reclaiming the lock (we let the tmcondvars leak)
    ~lock_tmcondvar_buffer_t() {
        pthread_mutex_destroy(&mutex);
    }

    /// safely insert an element via tmcondvars/locks
    void put(int value) {
        pthread_mutex_lock(&mutex);
        while (!buffer.not_full())
            tmcondvar_wait_lock(empty, &mutex);
        buffer.put_unchecked(value);
        tmcondvar_signal_lock(full);
        pthread_mutex_unlock(&mutex);
    }

    /// safely extract an element via tmcondvars/locks
    int get() {
        pthread_mutex_lock(&mutex);
        while (!buffer.not_empty())
            tmcondvar_wait_lock(full, &mutex);
        int ret = buffer.get_unchecked();
        tmcondvar_signal_lock(empty);
        pthread_mutex_unlock(&mutex);
        return ret;
    }
};

/// A buffer object synchronized via transactions and tmcondvars
///
/// This is the desired end-state
struct tm_tmcondvar_buffer_t : public synchronized_buffer_t
{
    tmcondvar*      empty;  /// condvar to indicate buffer has space for put
    tmcondvar*      full;   /// condvar to indicate buffer has content to get
    buffer_t        buffer; /// the buffer to use

    /// construct a buffer and set up tmcondvars
    tm_tmcondvar_buffer_t(int cap = 16, float preload_factor = 0.5)
        : buffer(cap, preload_factor)
    {
        empty = tmcondvar_create();
        full = tmcondvar_create();
    }

    /// destructor is a no-op.  We let the tmcondvars leak.
    ~tm_tmcondvar_buffer_t() { }

    /// safely insert an element via transactions and tmcondvars
    ///
    /// NB: we rely on checkput() since our current tmcondvar interface
    /// requires a call to tmcondvar_wait() to be the last shared memory
    /// operation in the transaction.  Using a helper function keeps it from
    /// getting too cumbersome.  This can be coded in fewer lines, but we're
    /// favoring readability.
    void put(int value) {
        while (true) {
            if (checkput(value))
                break;
        }
    }

    /// helper function to insert IFF there is room, and cond_wait otherwise.
    ///
    /// NB: carefully constructed so that cond_wait is the last instruction
    /// in its transaction
    bool checkput(int value) {
        __transaction_atomic {
            if (!buffer.not_full()) {
                tmcondvar_wait(empty);
            }
            else {
                buffer.put_unchecked(value);
                tmcondvar_signal(full);
                return true;
            }
        }
        return false;
    }

    /// safely extract an element via transactions and tmcondvars
    ///
    /// NB: we rely on checkget() since our current tmcondvar interface
    /// requires a call to condvar_wait() to be the last shared memory
    /// operation in the transaction.  Using a helper function keeps it from
    /// getting too cumbersome.  This can be coded in fewer lines, but we're
    /// favoring readability.
    int get() {
        int ret;
        while (true) {
            if (checkget(ret))
                break;
        }
        return ret;
    }

    /// helper function to extract IFF there's something in there, and
    /// cond_wait otherwise.
    ///
    /// NB: carefully constructed so that cond_wait is the last instruction
    /// in its transaction.
    bool checkget(int& val) {
        __transaction_atomic {
            if (!buffer.not_empty()) {
                tmcondvar_wait(full);
            }
            else {
                val = buffer.get_unchecked();
                tmcondvar_signal(empty);
                return true;
            }
        }
        return false;
    }
};
