// TODO: make sure this is clean, and commented, and then go back to
// memcached and handle it, using the new C interface.

#include <atomic>
#include <string>
#include <set>
#include <unistd.h>
#include <cassert>
#include <sys/time.h>
#include <signal.h>

#include "buffers.h"

/// benchmark configuration information
struct config_t
{
    int n_producers;            // # producer threads
    int n_consumers;            // # consumer threads
    int n_items_p;              // # items produced per producer
    int n_items_c;              // # items consumed per consumer
    int duration;               // how long to run experiment (seconds)
    int size;                   // maximum capacity of the buffer
    float preload_factor;       // How full to make the buffer
    std::set<int> bench_to_run; // Which benchmark to execute
    bool enable_time;           // true iff we're using duration instead of
                                // n_items[pc] to control benchmark

    /// constructor just sets defaults
    config_t()
        : n_producers(5), n_consumers(3), n_items_p(60), n_items_c(100),
          duration(10), size(10), preload_factor(0.5), enable_time(false)
    {
    }
};

/// provide basic usage information
void usage(char* progname) {
    printf("Usage: ./%s [-m 5] [-n 3] [-p 60] [-c 100] [-s 10] [-l]\n", progname);
    printf("  -h print help (this message)\n");
    printf("  -m the number of producer threads. default: 5\n");
    printf("  -n the number of consumer threads. default: 3\n");
    printf("  -p the number of items produced by each producer. default: 60\n");
    printf("  -c the number of itmes consumed by each consumer. default: 100\n");
    printf("  -t duration. default: 10s. overrides -p or -c option\n");
    printf("  -s the size of the bounded buffer. default: 10\n");
    printf("  -l the preload factor of buffer. default: 0.5\n\n");
    printf("  -r benchmark to run:\n");
    printf("     0: mutex + condvar (baseline)\n");
    printf("     1: mutex + semaphore\n");
    printf("     2: tm + semaphore\n");
    printf("     3: mutex + tmcondvar\n");
    printf("     4: tm + tmcondvar\n");
    exit(1);
}

/// store benchmark config here
static config_t bb;

/// parse arguments
void parseargs (int argc, char** argv) {
    opterr = 0;
    int c;
    while ((c = getopt(argc, argv, "m:n:p:c:t:s:l:r:h")) != -1) {
        switch(c) {
          case 'h':
            usage(argv[0]);
            break;
          case 'm':
            bb.n_producers = atoi(optarg);
            break;
          case 'n':
            bb.n_consumers = atoi(optarg);
            break;
          case 'p':
            bb.n_items_p = atoi(optarg);
            break;
          case 'c':
            bb.n_items_c = atoi(optarg);
            break;
          case 't':
            bb.duration = atoi(optarg);
            bb.enable_time = true;
            break;
          case 's':
            bb.size = atoi(optarg);
            break;
          case 'l':
            bb.preload_factor = atof(optarg);
            break;
          case 'r':
            bb.bench_to_run.insert(atoi(optarg));
            break;
          case '?':
            if (optopt == 'm' || optopt == 'n' || optopt == 'p' ||
                optopt == 'c' || optopt == 's' || optopt == 'l' ||
                optopt == 't' || optopt == 'r')
            {
                printf("Option -%c requires an argument.\n", optopt);
            }
            else if (isprint(optopt)) {
                printf("Unknown option -%c. \n", optopt);
            }
            else {
                printf("Unknown option character \\x%x. \n", optopt);
            }
            exit(1);
          default:
            usage(argv[0]);
        }
    }

    // print configuration info
    if (!bb.enable_time) {
        printf("\nprogram started with:\n");
        printf("%u producers, each producing %u items;\n"
               "%u consumers, each consuming %u items;\n"
               "bounded buffer with size = %u and preload factor = %f\n\n",
               bb.n_producers, bb.n_items_p, bb.n_consumers,
               bb.n_items_c, bb.size, bb.preload_factor);

        assert(bb.n_producers * bb.n_items_p == bb.n_consumers * bb.n_items_c);
    }
    else {
        printf("\nprogram started with:\n");
        printf("%u producers;\n"
               "%u consumers;\n"
               "%us duration;\n"
               "bounded buffer with size = %u and preload factor = %f\n\n",
               bb.n_producers, bb.n_consumers, bb.duration,
               bb.size, bb.preload_factor);
    }
}

/// The buffer we'll use for experiments
synchronized_buffer_t* buffer;
const char* bench_name;

/// Create a buffer, based on the command-line args
void create_buffer(int index) {
    switch (index) {
      case 0:
        buffer = new pthread_buffer_t(bb.size, bb.preload_factor);
        bench_name = "pthread_buffer_t";
        break;
      case 1:
        buffer = new lock_sem_buffer_t(bb.size, bb.preload_factor);
        bench_name = "lock_sem_buffer_t";
        break;
      case 2:
        buffer = new tm_sem_buffer_t(bb.size, bb.preload_factor);
        bench_name = "tm_sem_buffer_t";
        break;
      case 3:
        buffer = new lock_tmcondvar_buffer_t(bb.size, bb.preload_factor);
        bench_name = "lock_tmcondvar_buffer_t";
        break;
      case 4:
        buffer = new tm_tmcondvar_buffer_t(bb.size, bb.preload_factor);
        bench_name = "tm_tmcondvar_buffer_t";
        break;
      default:
        printf("Invalid benchmark selection\n");
        exit(1);
    }
}

/// for making sure all threads start/stop together
pthread_barrier_t barrier;

/// for controlling timed experiments
std::atomic<bool> running;

/// for measuring time
double time_begin, time_end;

/// SIGALRM handler for interrupting/ending a timed experiment
extern "C" void catch_SIGALRM (int) { running = false; }

/// this is the code each producer will run
void * producer(void * arg) {
    // per-thread initialization of tmcondvars, in case it's needed
    tmcondvar_thread_init();

    // random seed, for generating stuff to produce
    unsigned seed = (unsigned)((uintptr_t)arg);

    // wait on all threads, then thread 0 configures timing
    pthread_barrier_wait(&barrier);
    if ((int)(intptr_t)arg == 0) {
        // if timed experiment, set alarm
        if (bb.enable_time) {
            running = true;
            signal(SIGALRM, catch_SIGALRM);
            alarm(bb.duration);
        }

        // get start time
        struct timeval t;
        gettimeofday(&t,NULL);
        time_begin = (double)t.tv_sec+(double)t.tv_usec*1e-6;
    }

    // wait on all threads, then start experiment
    pthread_barrier_wait(&barrier);
    // fixed # operations:
    if (!bb.enable_time) {
        for(int i = 0; i < bb.n_items_p; ++i)
            buffer->put(rand_r(&seed));
    }
    // run for duration
    else {
        while (running) {
            buffer->put(rand_r(&seed));
        }
    }

    // wait on all threads, then get stop time and we're done
    pthread_barrier_wait(&barrier);
    if ((int)(intptr_t)arg == 0) {
        struct timeval t;
        gettimeofday(&t,NULL);
        time_end = (double)t.tv_sec+(double)t.tv_usec*1e-6;
    }
    return NULL;
}

/// this is the code each consumer will run
void * consumer(void * arg) {
    // per-thread initialization of tmcondvars, in case it's needed
    tmcondvar_thread_init();

    // wait while some producer gets things ready, then start consuming
    pthread_barrier_wait(&barrier);
    pthread_barrier_wait(&barrier);

    int tmp;
    // fixed # operations
    if (!bb.enable_time) {
        for(int i = 0; i < bb.n_items_c; ++i) {
            tmp = buffer->get();
        }
    }
    // run for duration
    else {
        while (running) {
            tmp = buffer->get();
        }
    }

    // all done
    pthread_barrier_wait(&barrier);
    return NULL;
}

/// configure and execute an experiment
void run() {
    // set up barrier
    pthread_barrier_init(&barrier, NULL, bb.n_producers + bb.n_consumers);

    // fork threads
    pthread_t threads[bb.n_producers + bb.n_consumers];
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    int i = 0;
    for(i = 0; i < bb.n_producers; ++i)
        pthread_create(&threads[i], &attr, producer, (void*)(intptr_t)i);
    for(i = bb.n_producers; i < bb.n_producers + bb.n_consumers; ++i)
        pthread_create(&threads[i], &attr, consumer, (void*)(intptr_t)i);

    // join threads
    for(i = 0; i < bb.n_producers + bb.n_consumers; ++i)
        pthread_join(threads[i], NULL);

    // clean up
    pthread_attr_destroy(&attr);
    pthread_barrier_destroy(&barrier);
}

/// Main just parses args and kicks off experiments
int main(int argc, char** argv) {
    parseargs(argc, argv);

    for (auto bench : bb.bench_to_run) {
        create_buffer(bench);
        run();
        printf("%s, Running time (s): %.3f\n", bench_name, time_end - time_begin);
        delete buffer;
    }
}
