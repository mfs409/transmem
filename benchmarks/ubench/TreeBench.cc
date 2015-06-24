// -*-c++-*-
//
//  Copyright (C) 2011, 2014
//  University of Rochester Department of Computer Science
//    and
//  Lehigh University Department of Computer Science and Engineering
//
// License: Modified BSD
//          Please see the file LICENSE for licensing information

#include "bmconfig.h"
#include "bmharness.h"
#include "Tree.h"

/// This is the tree we will manipulate in this experiment
benchmark<RBTree> SET;

/// This static, declared in bmconfig, needs to be defined
Config Config::CFG;

/// A helper function to update the configuration based on some custom names
void reparse_args() {
    if      (Config::CFG.bmname == "")          Config::CFG.bmname   = "RBTree";
    else if (Config::CFG.bmname == "RBTree16")  Config::CFG.elements = 16;
    else if (Config::CFG.bmname == "RBTree256") Config::CFG.elements = 256;
    else if (Config::CFG.bmname == "RBTree1K")  Config::CFG.elements = 1024;
    else if (Config::CFG.bmname == "RBTree64K") Config::CFG.elements = 65536;
    else if (Config::CFG.bmname == "RBTree1M")  Config::CFG.elements = 1048576;
}

/// We just call to SET functions in main
int main(int argc, char** argv) {
    // parse command line
    Config::CFG.parseargs(argc, argv, "TreeBench");
    reparse_args();

    // warm up the data structure
    SET.warmup();

    // run the tests
    SET.launch_test();

    // print results
    Config::CFG.dump_csv();
}
