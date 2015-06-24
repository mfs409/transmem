#include "bmconfig.h"
#include "bmharness.h"
#include "StdSet.h"

/// This is the tree we will manipulate in this experiment
benchmark<StdSet> SET;

/// This static, declared in bmconfig, needs to be defined
Config Config::CFG;

/// A helper function to update the configuration based on some custom names
void reparse_args() {
    if      (Config::CFG.bmname == "")          Config::CFG.bmname   = "StdSet";
    else if (Config::CFG.bmname == "StdSet")    Config::CFG.elements = 256;
    else if (Config::CFG.bmname == "StdSet16")  Config::CFG.elements = 16;
    else if (Config::CFG.bmname == "StdSet256") Config::CFG.elements = 256;
    else if (Config::CFG.bmname == "StdSet1K")  Config::CFG.elements = 1024;
    else if (Config::CFG.bmname == "StdSet64K") Config::CFG.elements = 65536;
    else if (Config::CFG.bmname == "StdSet1M")  Config::CFG.elements = 1048576;
}

/// We just call to SET functions in main
int main(int argc, char** argv) {
    // parse command line
    Config::CFG.parseargs(argc, argv, "StdSetBench");
    reparse_args();

    // warm up the data structure
    SET.warmup();

    // run the tests
    SET.launch_test();

    // print results
    Config::CFG.dump_csv();
}
