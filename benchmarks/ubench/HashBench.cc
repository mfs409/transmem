// -*-c++-*-
//
//  Copyright (C) 2011, 2015
//  University of Rochester Department of Computer Science
//    and
//  Lehigh University Department of Computer Science and Engineering
//
// License: Modified BSD
//          Please see the file LICENSE for licensing information

#include "bmconfig.h"
#include "bmharness.h"
#include "Hash.h"

/// This is the hash table we'll manipulate in the experiment
benchmark<HashTable> SET;

/// This static, declared in bmconfig, needs to be defined
Config Config::CFG;

/// A helper function to update the configuration based on some custom names
void reparse_args() {
    if (Config::CFG.bmname == "") Config::CFG.bmname = "Hash";
}

/// We just call to SET functions in main
int main(int argc, char** argv) {
    // parse command line
    Config::CFG.parseargs(argc, argv, "HashBench");
    reparse_args();

    // warm up the data structure
    SET.warmup();

    // run the tests
    SET.launch_test();

    // print results
    Config::CFG.dump_csv();
}
