// -*-c++-*-

#pragma once

#include <iostream>
#include <atomic>
#include <string>
#include <unistd.h>

/**
 * Standard benchmark configuration globals
 */
struct Config
{
    /*** THESE GET WRITTEN EARLY ***/
    std::string bmname;
    uint32_t    duration;               /// in seconds
    uint32_t    execute;                /// in transactions
    uint32_t    threads;                /// number of threads
    uint32_t    nops_after_tx;          /// self-explanatory
    uint32_t    elements;               /// elements in data structure
    uint32_t    lookpct;                /// lookup percent
    uint32_t    inspct;                 /// insert percent
    uint32_t    sets;                   /// number of sets to create
    uint32_t    ops;                    /// operations per transaction

    /*** THESE GET UPDATED LATER ***/
    std::atomic<uint64_t> time;            /// total time the test ran
    std::atomic<bool>     running;         /// is the test still running
    std::atomic<uint32_t> txcount;         /// total transactions
    std::atomic<int32_t>  lookup_hit;      /// total successful lookup txns
    std::atomic<int32_t>  lookup_miss;     /// total unsuccessful lookup txns
    std::atomic<int32_t>  insert_hit;      /// total successful insert txns
    std::atomic<int32_t>  insert_miss;     /// total unsuccessful insert txns
    std::atomic<int32_t>  remove_hit;      /// total successful remove txns
    std::atomic<int32_t>  remove_miss;     /// total unsuccessful remove txns

    /// Constructor just sets reasonable defaults for everything
    Config() :
        bmname(""),
        duration(1),   execute(0),
        threads(1),    nops_after_tx(0),
        elements(256), lookpct(34),
        inspct(66),    sets(1),
        ops(1),        time(0),
        running(true), txcount(0),
        lookup_hit(0), lookup_miss(0),
        insert_hit(0), insert_miss(0),
        remove_hit(0), remove_miss(0)
    { }

    /// Print benchmark configuration output
    void dump_csv() {
        // csv output
        std::cout << "csv"
                  << ", B=" << bmname     << ", R=" << lookpct
                  << ", d=" << duration   << ", p=" << threads
                  << ", X=" << execute    << ", m=" << elements
                  << ", S=" << sets       << ", O=" << ops
                  << ", txns=" << txcount << ", time=" << time
                  << ", throughput="
                  << (1000000000LL * txcount) / (time)
                  << std::endl;
        std::cout << "(l:"  << lookup_hit << "/" << lookup_miss
                  << ", i:" << insert_hit << "/" << insert_miss
                  << ", r:" << remove_hit << "/" << remove_miss
                  << ")" << std::endl;
    }

    /// Print usage
    void usage(std::string name) {
        std::cerr << "Usage: " << name << " -C <stm algorithm> [flags]\n";
        std::cerr << "    -d: number of seconds to time (default 1)\n";
        std::cerr << "    -X: execute fixed tx count, not for a duration\n";
        std::cerr << "    -p: number of threads (default 1)\n";
        std::cerr << "    -N: nops between transactions (default 0)\n";
        std::cerr << "    -R: % lookup txns (remainder split ins/rmv)\n";
        std::cerr << "    -m: range of keys in data set\n";
        std::cerr << "    -B: name of benchmark\n";
        std::cerr << "    -S: number of sets to build (default 1)\n";
        std::cerr << "    -O: operations per transaction (default 1)\n";
        std::cerr << "    -h: print help (this message)\n\n";
    }

    /// Parse command line arguments
    void parseargs(int argc, char** argv, std::string name) {
        int opt;
        while ((opt = getopt(argc, argv, "N:d:p:hX:B:m:R:S:O:")) != -1) {
            switch(opt) {
              case 'd': duration      = strtol(optarg, NULL, 10); break;
              case 'p': threads       = strtol(optarg, NULL, 10); break;
              case 'N': nops_after_tx = strtol(optarg, NULL, 10); break;
              case 'X': execute       = strtol(optarg, NULL, 10); break;
              case 'B': bmname        = std::string(optarg); break;
              case 'm': elements      = strtol(optarg, NULL, 10); break;
              case 'S': sets          = strtol(optarg, NULL, 10); break;
              case 'O': ops           = strtol(optarg, NULL, 10); break;
              case 'R':
                lookpct = strtol(optarg, NULL, 10);
                inspct = (100 - lookpct)/2 + strtol(optarg, NULL, 10);
                break;
              case 'h':
                usage(name);
            }
        }
    }

    /// we can call this from an alarm signal handler to stop the experiment
    static void catch_SIGALRM(int) {
        CFG.running = false;
    }

    /// Singleton for the configuration, so we can see it from the alarm
    /// handler
    static Config CFG;
};
