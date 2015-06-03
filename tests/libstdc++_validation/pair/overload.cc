#include <iostream>
#include <string>
#include <utility>
#include <cassert>
#include "tests.h"
#include "verify.h"

/// The pair we will use for our tests
typedef std::pair<int, int> intpair;
static intpair * overload_pair = NULL;

void overload_tests(int id)
{
    std::cout << "  Overloaded operators are untraceable\n";
    std::cout << "  Get functions are untraceable\n";

    // test swap (external)
    global_barrier->arrive(id);
    {
        verifier v;
        intpair p(1, 2);
        BEGIN_TX;
        overload_pair = new intpair(3, 4);
        swap(*overload_pair, p);
        v.insert_all<intpair>(overload_pair);
        delete(overload_pair);
        overload_pair = NULL;
        END_TX;
        v.check("pair swap (external)", id, 2, {1, 2});
    }
}
