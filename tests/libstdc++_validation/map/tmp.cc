/*
  Driver to test the transactional version of the std::map interface

  According to http://www.cplusplus.com/reference/map/map/, the
  std::map interface consists of the following:

  Steps:
    1 - Put TBD traces into all of GCC's map functions
    2 - For one category at a time, replace TBDs and update columns 3 and 4
        of table below
    3 - Write test code for ensuring that every traced function is called,
        and then write DONE in the category

|-------------------+-----------------------+--------------------+------------------------|
| Category          | Functions             | C++14              |                    GCC |
|                   |                       | Expected           |                 Actual |
|-------------------+-----------------------+--------------------+------------------------|
| Member Functions  | (constructor)         | 1a, 1b, 1c, 2a, 2b | 1a, 1b, 1c, 2a, 2b, 2c |
| (DONE)            |                       | 3a, 3b, 4a, 4b     |         3a, 3b, 4a, 4b |
|                   |                       | 5a, 5b             |                 5a, 5b |
|                   | (destructor)          | 1                  |                   NONE |
|                   | operator=             | 1, 2, 3            |                1, 2, 3 |
|-------------------+-----------------------+--------------------+------------------------|
| Iterators         | begin                 | 1a, 1b             |                 1a, 1b |
| (DONE)            | end                   | 1a, 1b             |                 1a, 1b |
|                   | rbegin                | 1a, 1b             |                 1a, 1b |
|                   | rend                  | 1a, 1b             |                 1a, 1b |
|                   | cbegin                | 1                  |                      1 |
|                   | cend                  | 1                  |                      1 |
|                   | crbegin               | 1                  |                      1 |
|                   | crend                 | 1                  |                      1 |
|-------------------+-----------------------+--------------------+------------------------|
| Iterator          | default constructable |                    |                        |
| Methods           | copy constructable    |                    |                        |
|                   | copy assignable       |                    |                        |
|                   | destructible          |                    |                        |
|                   | swappable             |                    |                        |
|-------------------+-----------------------+--------------------+------------------------|
| Iterator          | operator*             |                    |                        |
| Operators         | operator->            |                    |                        |
|                   | operator++            |                    |                        |
|                   | operator--            |                    |                        |
|                   | operator+=            |                    |                        |
|                   | operator+             |                    |                        |
|                   | operator-=            |                    |                        |
|                   | operator-             |                    |                        |
|                   | operator[]            |                    |                        |
|-------------------+-----------------------+--------------------+------------------------|
| Iterator          | operator==            |                    |                        |
| Overloads         | operator!=            |                    |                        |
|                   | operator<             |                    |                        |
|                   | operator>             |                    |                        |
|                   | operator<=            |                    |                        |
|                   | operator>=            |                    |                        |
|                   | operator-             |                    |                        |
|                   | operator+             |                    |                        |
|-------------------+-----------------------+--------------------+------------------------|
| Const             |                       |                    |                        |
| Iterator          |                       |                    |                        |
| Methods           |                       |                    |                        |
|-------------------+-----------------------+--------------------+------------------------|
| Reverse           |                       |                    |                        |
| Iterator          |                       |                    |                        |
| Methods           |                       |                    |                        |
|-------------------+-----------------------+--------------------+------------------------|
| Const Reverse     |                       |                    |                        |
| Iterator          |                       |                    |                        |
| Methods           |                       |                    |                        |
|-------------------+-----------------------+--------------------+------------------------|
| Capacity          | empty                 | 1                  |                      1 |
| (DONE)            | size                  | 1                  |                      1 |
|                   | max_size              | 1                  |                      1 |
|-------------------+-----------------------+--------------------+------------------------|
| Element           | operator[]            | 1a, 1b             |                 1a, 1b |
| Access            | at                    | 1a, 1b             |                 1a, 1b |
| (DONE)            |                       |                    |                        |
|-------------------+-----------------------+--------------------+------------------------|
| Modifiers         | insert                | 1a, 1b, 2a, 2b     |         1a, 1b, 2a, 2b |
| (DONE)            |                       | 3, 4               |                   3, 4 |
|                   | erase                 | 1, 2, 3            |           1a, 1b, 2, 3 |
|                   | swap                  | 1                  |                      1 |
|                   | clear                 | 1                  |                      1 |
|                   | emplace               | 1                  |                      1 |
|                   | emplace_hint          | 1                  |                      1 |
|-------------------+-----------------------+--------------------+------------------------|
| Observers         | get_allocator         | 1                  |                      1 |
| (DONE)            | key_comp              | 1                  |                      1 |
|                   | value_comp            | 1                  |                      1 |
|-------------------+-----------------------+--------------------+------------------------|
| Operations        | find                  | 1a, 1b             |                 1a, 1b |
| (DONE)            | count                 | 1                  |                      1 |
|                   | lower_bound           | 1a, 1b             |                 1a, 1b |
|                   | upper_bound           | 1a, 1b             |                 1a, 1b |
|                   | equal_range           | 1a, 1b             |                 1a, 1b |
|-------------------+-----------------------+--------------------+------------------------|
| Non-member        | '=='                  |                    |                      1 |
| function          | '!='                  |                    |                      1 |
| overloads (NMFOs) | '<'                   |                    |                      1 |
| (DONE)            | '<='                  |                    |                      1 |
|                   | '>'                   |                    |                      1 |
|                   | '>='                  |                    |                      1 |
|                   | swap                  |                    |                      1 |
|-------------------+-----------------------+--------------------+------------------------|
*/

#include <list>
#include <cstdio>
#include <thread>
#include <atomic>
#include <mutex>
#include <cassert>
#include <iostream>
#include <unistd.h>

#include "../common/barrier.h"
#include "tests.h"

using std::cout;
using std::endl;

/// configured via command line args: number of threads
int  num_threads = 1;

/// the barrier to use when we are in concurrent mode
barrier* global_barrier;

/// the mutex to use when we are in concurrent mode with tm turned off
std::mutex global_mutex;

/// Report on how to use the command line to configure this program
void usage()
{
    cout << "Command-Line Options:" << endl
         << "  -n <int> : specify the number of threads" << endl
         << "  -h       : display this message" << endl
         << "  -T       : enable all tests" << endl
         << "  -t <int> : enable a specific test" << endl
         << "               1 constructors and destructors" << endl
         << "               2 operator=" << endl
         << "               3 iterator creation" << endl
         << "               4 iterator methods" << endl
         << "               5 iterator operators" << endl
         << "               6 iterator overloads" << endl
         << "               7 iterator functions" << endl
         << "               8 capacity methods" << endl
         << "               9 element access methods" << endl
         << "              10 modifier methods" << endl
         << "              11 observer methods" << endl
         << "              12 operations methods" << endl
         << "              13 overload functions" << endl
         << endl
         << "  Note: const, reverse, and const reverse iterators not tested"
         << endl
         << endl;
    exit(0);
}

#define NUM_TESTS 14
bool test_flags[NUM_TESTS] = {false};

void (*test_names[NUM_TESTS])(int) = {
    NULL,
    ctor_dtor_tests,                                    // member.cc
    op_eq_tests,                                        // member.cc
    iter_create_tests,                                  // iter.cc
    iter_method_tests,                                  // iter.cc
    iter_operator_tests,                                // iter.cc
    iter_overload_tests,                                // iter.cc
    iter_function_tests,                                // iter.cc
    cap_tests,                                          // cap.cc
    element_tests,                                      // element.cc
    modifier_tests,                                     // modifier.cc
    observer_tests,                                     // observer.cc
    operation_tests,                                    // operations.cc
    overload_tests                                      // overloads.cc
};

/// Parse command line arguments using getopt()
void parseargs(int argc, char** argv)
{
    // parse the command-line options
    int opt;
    while ((opt = getopt(argc, argv, "n:ht:")) != -1) {
        switch (opt) {
          case 'n': num_threads = atoi(optarg); break;
          case 'h': usage();                    break;
          case 't': test_flags[atoi(optarg)] = true; break;
          case 'T': for (int i = 0; i < NUM_TESTS; ++i) test_flags[i] = true; break;
        }
    }
}

/// A concurrent test for exercising every method of std::list.  This is
/// called by every thread
void per_thread_test(int id)
{
    // wait for all threads to be ready
    global_barrier->arrive(id);

    // run the tests that were requested on the command line
    for (int i = 0; i < NUM_TESTS; ++i)
        if (test_flags[i])
            test_names[i](id);
}

/// main() just parses arguments, makes a barrier, and starts threads
int main(int argc, char** argv)
{
    // figure out what we're doing
    parseargs(argc, argv);

    // set up the barrier
    global_barrier = new barrier(num_threads);

    // make threads
    std::thread* threads = new std::thread[num_threads];
    for (int i = 0; i < num_threads; ++i)
        threads[i] = std::thread(per_thread_test, i);

    // wait for the threads to finish
    for (int i = 0; i < num_threads; ++i)
        threads[i].join();
}
#include <iostream>
#include <map>
#include "tests.h"
#include "verify.h"

/// The maps we will use for our tests
std::map<int, int>* member_map = NULL;
// NB: we must make the comparison function transaction_safe, or we'll get
//     lots of errors.  This is slightly annoying.
typedef bool(*safecomp)(int, int) __attribute__((transaction_safe));
std::map<int, int, safecomp>* member_map_comparison = NULL;

// a comparison function
bool intcomp(int lhs, int rhs) { return lhs < rhs;}

void ctor_dtor_tests(int id)
{
    // print simple output
    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing member map constructors(12/11) and destructors(0/1)\n");

    // set up our comparison function pointer:
    safecomp comparison = intcomp;

    // test simple ctor and dtor
    //
    // NB: we haven't actually verified size yet, but we use it here.
    global_barrier->arrive(id);
    {
        map_verifier v;
        int size;
        BEGIN_TX;
        member_map = new std::map<int, int>();
        int size = member_map->size();
        delete(member_map);
        END_TX;
        v.check_size("basic ctor(1a) and dtor(0)", id, size);
    }

    // test construction with explicit comparison function
    global_barrier->arrive(id);
    {
        map_verifier v;
        int size;
        BEGIN_TX;
        member_map_comparison = new std::map<int, int, safecomp>(comparison);
        int size = member_map_comparison->size();
        delete(member_map_comparison);
        END_TX;
        v.check_size("basic ctor(1b)", id, size);
    }

    // test ctor with explicit allocator
    global_barrier->arrive(id);
    {
        map_verifier v;
        int size;
        std::map<int, int> tmp;
        BEGIN_TX;
        member_map = new std::map<int, int>(tmp.get_allocator());
        int size = member_map->size();
        delete(member_map);
        END_TX;
        v.check_size("basic ctor(1c)", id, size);
    }

    // test range ctor with explicit comparison function
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> tmp({{1, 1}, {2, 2}, {3, 3}});
        BEGIN_TX;
        member_map_comparison = new std::map<int, int, safecomp>(tmp.begin(), tmp.end(), comparison);
        v.insert_all(member_map_comparison);
        delete(member_map_comparison);
        END_TX;
        v.check("range ctor(2a)", id, 6, {1, 1, 2, 2, 3, 3});
    }

    // test range ctor with explicit allocator function
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> tmp({{1, 1}, {2, 2}, {3, 3}});
        BEGIN_TX;
        member_map = new std::map<int, int>(tmp.begin(), tmp.end(), tmp.get_allocator());
        v.insert_all(member_map);
        delete(member_map);
        END_TX;
        v.check("range ctor(2b)", id, 6, {1, 1, 2, 2, 3, 3});
    }

    // test range ctor with implicit allocator and comparison functions
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> tmp({{1, 1}, {2, 2}, {3, 3}});
        BEGIN_TX;
        member_map = new std::map<int, int>(tmp.begin(), tmp.end());
        v.insert_all(member_map);
        delete(member_map);
        END_TX;
        v.check("range ctor(2c)", id, 6, {1, 1, 2, 2, 3, 3});
    }

    // test copy ctor
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> tmp({{1, 1}, {2, 2}, {3, 3}});
        BEGIN_TX;
        member_map = new std::map<int, int>(tmp);
        v.insert_all(member_map);
        delete(member_map);
        END_TX;
        v.check("copy ctor(3a)", id, 6, {1, 1, 2, 2, 3, 3});
    }

    // test copy ctor with explicit allocator
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> tmp({{1, 1}, {2, 2}, {3, 3}});
        BEGIN_TX;
        member_map = new std::map<int, int>(tmp, tmp.get_allocator());
        v.insert_all(member_map);
        delete(member_map);
        END_TX;
        v.check("copy ctor(3b)", id, 6, {1, 1, 2, 2, 3, 3});
    }

    // test move ctor
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> tmp({{1, 1}, {2, 2}, {3, 3}});
        BEGIN_TX;
        member_map = new std::map<int, int>(std::move(tmp));
        v.insert_all(member_map);
        delete(member_map);
        END_TX;
        v.check("move ctor(4a)", id, 6, {1, 1, 2, 2, 3, 3});
    }

    // test move ctor with explicit allocator
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> tmp({{1, 1}, {2, 2}, {3, 3}});
        BEGIN_TX;
        member_map = new std::map<int, int>(std::move(tmp), tmp.get_allocator());
        v.insert_all(member_map);
        delete(member_map);
        END_TX;
        v.check("copy ctor(4b)", id, 6, {1, 1, 2, 2, 3, 3});
    }

    // test initializer list ctor with explicit comparison function
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        member_map_comparison = new std::map<int, int, safecomp>({{1, 1}, {2, 2}, {3, 3}}, comparison);
        v.insert_all(member_map_comparison);
        delete(member_map_comparison);
        END_TX;
        v.check("ilist ctor(5a)", id, 6, {1, 1, 2, 2, 3, 3});
    }

    // test initializer list ctor with default comparison function
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> tmp({{1, 1}, {2, 2}, {3, 3}});
        BEGIN_TX;
        member_map = new std::map<int, int>({{1, 1}, {2, 2}, {3, 3}}, tmp.get_allocator());
        v.insert_all(member_map);
        delete(member_map);
        END_TX;
        v.check("ilist ctor(5b)", id, 6, {1, 1, 2, 2, 3, 3});
    }
}

void op_eq_tests(int id)
{
    // print simple output
    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing member map operator=(3)\n");

    // test #1 is operator= copy
    //
    // NB: we haven't actually verified iterators yet, but we use them...
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> local = { {1, 1}, {4, 4}, {2, 2}};
        BEGIN_TX;
        member_map = new std::map<int, int>();
        *member_map = local;
        v.insert_all<std::map<int, int>>(member_map);
        delete(member_map);
        member_map = NULL;
        END_TX;
        v.check("copy operator= (1)", id, 6, {1, 1, 2, 2, 4, 4, -2});
    }

    // test #2 is operator= move
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> local = { {1, 1}, {4, 4}, {2, 2}};
        BEGIN_TX;
        member_map = new std::map<int, int>();
        *member_map = std::move(local);
        v.insert_all<std::map<int, int>>(member_map);
        delete(member_map);
        member_map = NULL;
        END_TX;
        v.check("move operator= (2)", id, 6, {1, 1, 2, 2, 4, 4, -2});
    }

    // test #3 is operator= ilist
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        member_map = new std::map<int, int>();
        *member_map = { {1, 1}, {4, 4}, {2, 2}};
        v.insert_all<std::map<int, int>>(member_map);
        delete(member_map);
        member_map = NULL;
        END_TX;
        v.check("ilist operator= (3)", id, 6, {1, 1, 2, 2, 4, 4, -2});
    }
}
#include <iostream>
#include <map>
#include "tests.h"
#include "verify.h"

std::map<int, int>* iter_map = NULL;

void iter_create_tests(int id)
{
    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing iterator begin/end functions\n");

    // the first test will simply ensure that we can call begin() and end()
    // correctly
    global_barrier->arrive(id);
    {
        bool ok = false;
        map_verifier v;
        BEGIN_TX;
        iter_map = new std::map<int, int>();
        std::map<int, int>::iterator b = iter_map->begin();
        std::map<int, int>::iterator e = iter_map->end();
        ok = (b == e);
        delete(iter_map);
        iter_map = NULL;
        END_TX;
        if (!ok)
            printf(" [%d] iterator begin and end did not match for empty deque", id);
        else if (id == 0)
            printf(" [OK] %s\n", "basic iterator begin and end");
    }

    // now test the legacy const begin() and end() calls
    global_barrier->arrive(id);
    {
        bool ok = false;
        map_verifier v;
        BEGIN_TX;
        iter_map = new std::map<int, int>();
        const std::map<int, int>* cd = iter_map;
        std::map<int, int>::const_iterator b = cd->begin();
        std::map<int, int>::const_iterator e = cd->end();
        ok = (b == e);
        delete(iter_map);
        iter_map = NULL;
        END_TX;
        if (!ok)
            printf(" [%d] iterator begin and end did not match for empty deque", id);
        else if (id == 0)
            printf(" [OK] %s\n", "legacy const iterator begin and end");
    }

    // the first test will simply ensurec that we can call begin() and end()
    // correctly
    global_barrier->arrive(id);
    {
        bool ok = false;
        map_verifier v;
        BEGIN_TX;
        iter_map = new std::map<int, int>();
        std::map<int, int>::reverse_iterator b = iter_map->rbegin();
        std::map<int, int>::reverse_iterator e = iter_map->rend();
        ok = (b == e);
        delete(iter_map);
        iter_map = NULL;
        END_TX;
        if (!ok)
            printf(" [%d] iterator rbegin and rend did not match for empty deque", id);
        else if (id == 0)
            printf(" [OK] %s\n", "basic reverse iterator rbegin and rend");
    }

    // now test the legacy const rbegin() and rend() calls
    global_barrier->arrive(id);
    {
        bool ok = false;
        map_verifier v;
        BEGIN_TX;
        iter_map = new std::map<int, int>();
        const std::map<int, int>* cd = iter_map;
        std::map<int, int>::const_reverse_iterator b = cd->rbegin();
        std::map<int, int>::const_reverse_iterator e = cd->rend();
        ok = (b == e);
        delete(iter_map);
        iter_map = NULL;
        END_TX;
        if (!ok)
            printf(" [%d] iterator rbegin and rend did not match for empty deque", id);
        else if (id == 0)
            printf(" [OK] %s\n", "legacy const reverse iterator rbegin and rend");
    }

    // now test the c++11 const begin() and end() calls
    global_barrier->arrive(id);
    {
        bool ok = false;
        map_verifier v;
        BEGIN_TX;
        iter_map = new std::map<int, int>();
        const std::map<int, int>* cd = iter_map;
        std::map<int, int>::const_iterator b = cd->cbegin();
        std::map<int, int>::const_iterator e = cd->cend();
        ok = (b == e);
        delete(iter_map);
        iter_map = NULL;
        END_TX;
        if (!ok)
            printf(" [%d] iterator cbegin and cend did not match for empty deque", id);
        else if (id == 0)
            printf(" [OK] %s\n", "c++11 cbegin and cend");
    }

    // now test the c++11 crbegin() and crend() calls
    global_barrier->arrive(id);
    {
        bool ok = false;
        map_verifier v;
        BEGIN_TX;
        iter_map = new std::map<int, int>();
        const std::map<int, int>* cd = iter_map;
        std::map<int, int>::const_reverse_iterator b = cd->crbegin();
        std::map<int, int>::const_reverse_iterator e = cd->crend();
        ok = (b == e);
        delete(iter_map);
        iter_map = NULL;
        END_TX;
        if (!ok)
            printf(" [%d] iterator crbegin and crend did not match for empty deque", id);
        else if (id == 0)
            printf(" [OK] %s\n", "c++11 crbegin and crend");
    }
}

void iter_method_tests(int id)
{
}

void iter_operator_tests(int id)
{
}

void iter_overload_tests(int id)
{
}

void iter_function_tests(int id)
{
}
#include <iostream>
#include <map>
#include "tests.h"
#include "verify.h"

/// The maps we will use for our tests
std::map<int, int>* cap_map = NULL;

/// test the capacity methods of std::map
void cap_tests(int id)
{
    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing map capacity functions: size(1), max_size(1), empty(1)\n");

    // #1: Test size()
    global_barrier->arrive(id);
    {
        int size = 0;
        BEGIN_TX;
        cap_map = new std::map<int, int>({{1, 1}, {2, 2}, {3, 3}});
        size = cap_map->size();
        delete(cap_map);
        END_TX;
        if (size != 3)
            printf(" [%d] map size test failed\n", id);
        else if (id == 0)
            printf(" [OK] %s\n", "map size()");
    }

    // #2: Test max_size()
    global_barrier->arrive(id);
    {
        int size = 0;
        BEGIN_TX;
        cap_map = new std::map<int, int>({{1, 1}, {2, 2}, {3, 3}});
        size = cap_map->max_size();
        delete(cap_map);
        END_TX;
        if (size < 1024)
            printf(" [%d] map max_size test failed %d\n", id, size);
        else if (id == 0)
            printf(" [OK] map::max_size == %d\n", size);
    }

    // #3: test empty()
    global_barrier->arrive(id);
    {
        bool ok = true;
        BEGIN_TX;
        cap_map = new std::map<int, int>({{1, 1}, {2, 2}, {3, 3}});
        ok &= !cap_map->empty();
        cap_map->clear();
        ok &= cap_map->empty();
        delete(cap_map);
        END_TX;
        if (!ok)
            printf(" [%d] map empty test failed\n", id);
        else if (id == 0)
            printf(" [OK] %s\n", "map empty()");
    }
}
#include <iostream>
#include <map>
#include "tests.h"

/// The maps we will use for our tests
std::map<int, int>* element_map = NULL;

void element_tests(int id)
{
    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing map element access functions: [](2), at(2)\n");

    // Test [] with const key
    global_barrier->arrive(id);
    {
        int ans = -2;
        BEGIN_TX;
        element_map = new std::map<int, int>({{1, 1}, {3, 3}, {2, 2}});
        const int x = 1;
        ans = (*element_map)[x];
        delete(element_map);
        END_TX;
        if (ans != 1)
            printf(" [%d] map operator[] test failed\n", id);
        else if (id == 0)
            printf(" [OK] %s\n", "map operator[](1a)");
    }

    // Test [] with move key
    global_barrier->arrive(id);
    {
        int ans = -2;
        BEGIN_TX;
        element_map = new std::map<int, int>({{1, 1}, {3, 3}, {2, 2}});
        int x = 4;
        ans = (*element_map)[std::move(x)];
        delete(element_map);
        END_TX;
        if (ans != 0)
            printf(" [%d] map operator[] test failed\n", id);
        else if (id == 0)
            printf(" [OK] %s\n", "map operator[](1b)");
    }

    // Test at() with non-const map
    global_barrier->arrive(id);
    {
        int ans = -2;
        BEGIN_TX;
        element_map = new std::map<int, int>({{1, 1}, {3, 3}, {2, 2}});
        ans = element_map->at(1);
        delete(element_map);
        END_TX;
        if (ans != 1)
            printf(" [%d] map at() test failed\n", id);
        else if (id == 0)
            printf(" [OK] %s\n", "map at(1a)");
    }

    // Test at() with const map
    global_barrier->arrive(id);
    {
        int ans = -2;
        BEGIN_TX;
        element_map = new std::map<int, int>({{1, 1}, {3, 3}, {2, 2}});
        const std::map<int, int> ce = *element_map;
        ans = ce.at(3);
        delete(element_map);
        END_TX;
        if (ans != 3)
            printf(" [%d] map at() test failed\n", id);
        else if (id == 0)
            printf(" [OK] %s\n", "map at(1b)");
    }
}
#include <iostream>
#include <map>
#include "tests.h"
#include "verify.h"

/// The maps we will use for our tests
std::map<int, int>* modifier_map = NULL;
struct Foo
{
    int x;
  private:
    explicit Foo(){};
  public:
    Foo(int z) : x(z) { }
};
std::map<int, Foo>* modifier_map_special = NULL;

void modifier_tests(int id)
{
    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing map modifier functions: insert(6), erase(4), "
               "swap(1), clear(1), emplace(1), emplace_hint(1)\n");

    // test insert of single element value_type (1a)
    //
    // mfs: I can't get this to instantiate 1a
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        modifier_map->insert({3, 3});
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("single element insert (1a)", id, 10, {1, 1, 2, 2, 3, 3, 4, 4, 8, 8, -2});
    }

    // test insert of single element via pair (1b)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        modifier_map->insert(std::pair<int, int>(3, 3));
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("single element insert (1b)", id, 10, {1, 1, 2, 2, 3, 3, 4, 4, 8, 8, -2});
    }

    // test insert with hint (2a)
    //
    // [mfs] I can't get this to instantiate 2a
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        auto i = modifier_map->begin(); i++;
        modifier_map->insert(i, {3, 3});
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("single element insert with hint (2a)", id, 10, {1, 1, 2, 2, 3, 3, 4, 4, 8, 8, -2});
    }

    // test insert with hint (2b)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        auto i = modifier_map->begin(); i++;
        modifier_map->insert(i, std::pair<int, int>(3, 3));
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("single element insert with hint (2b)", id, 10, {1, 1, 2, 2, 3, 3, 4, 4, 8, 8, -2});
    }

    // test insert with range
    global_barrier->arrive(id);
    {
        map_verifier v;
        std::map<int, int> tmp({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        BEGIN_TX;
        modifier_map = new std::map<int, int>();
        modifier_map->insert(tmp.begin(), tmp.end());
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("insert range (3)", id, 8, {1, 1, 2, 2, 4, 4, 8, 8, -2});
    }

    // test insert with initializer list (4)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>();
        modifier_map->insert({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("insert ilist (4)", id, 8, {1, 1, 2, 2, 4, 4, 8, 8, -2});
    }

    // test single element erase (1a)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{16, 16}, {4, 4}, {9, 9}, {25, 25}});
        auto i = modifier_map->cbegin();i++;i++;
        modifier_map->erase(i);
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("erase single element (1a)", id, 6, {4, 4, 9, 9, 25, 25});
    }

    // test single element erase (1b)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{16, 16}, {4, 4}, {9, 9}, {25, 25}});
        auto i = modifier_map->begin();i++;i++;
        modifier_map->erase(i);
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("erase single element (1b)", id, 6, {4, 4, 9, 9, 25, 25});
    }

    // test erase key (2)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{16, 16}, {4, 4}, {9, 9}, {25, 25}});
        modifier_map->erase(16);
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("erase by key (2)", id, 6, {4, 4, 9, 9, 25, 25});
    }

    // test erase range (3)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{16, 16}, {4, 4}, {9, 9}, {25, 25}});
        auto i = modifier_map->begin();i++;i++;
        modifier_map->erase(modifier_map->begin(), i);
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("erase range (3)", id, 4, {16, 16, 25, 25});
    }

    // test swap (1)
    global_barrier->arrive(id);
    {
        map_verifier v;
        int tmp = 0;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{1, 1}, {2, 2}, {3, 3}});
        std::map<int, int> swapper = {{4, 4}, {6, 6}};
        modifier_map->swap(swapper);
        for (auto i = swapper.begin(); i != swapper.end(); ++i)
            tmp = tmp * 10 + i->first;
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        if (tmp != 123)
            std::cout << "["<<id<<"] error in swap()" << std::endl;
        v.check("swap (1)", id, 4, {4, 4, 6, 6});
    }

    // test clear (1)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{1, 1}, {2, 2}, {3, 3}});
        modifier_map->clear();
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("clear (1)", id, 0, {-2, -2});
    }

    // test emplace (1)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{1, 1}, {2, 2}, {3, 3}});
        modifier_map->emplace(8, 8);
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("emplace (1)", id, 8, {1, 1, 2, 2, 3, 3, 8, 8, -2});
    }


    // test emplace_hint (1)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        modifier_map = new std::map<int, int>({{1, 1}, {8, 8}, {7, 7}});
        auto i = modifier_map->end();
        modifier_map->emplace_hint(i, 30, 30);
        v.insert_all(modifier_map);
        delete(modifier_map);
        END_TX;
        v.check("emplace_hint (1)", id, 8, {1, 1, 7, 7, 8, 8, 30, 30});
    }
}

#include <iostream>
#include <map>
#include "tests.h"
#include "verify.h"

/// The map we will use for our tests
std::map<int, int>* observer_map = NULL;

void observer_tests(int id)
{
    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing get_allocator(1), key_comp(1), value_comp(1)\n");

    // test get_allocator
    global_barrier->arrive(id);
    {
        map_verifier v;
        int size;
        BEGIN_TX;
        observer_map = new std::map<int, int>();
        auto a = observer_map->get_allocator();
        size = observer_map->size();
        delete(observer_map);
        observer_map = NULL;
        END_TX;
        v.check_size("get_allocator (1)", id, size);
    }

    // test keycomp and value_comp
    global_barrier->arrive(id);
    {
        bool ok = true;
        int a;
        BEGIN_TX;
        observer_map = new std::map<int, int>();
        auto k = observer_map->key_comp();
        auto v = observer_map->value_comp();
        ok &= k(1, 2);
        ok &= v(std::make_pair(1, 1), std::make_pair(2, 2));
        delete(observer_map);
        END_TX;
        if (!ok)
            std::cout << "["<<id<<"] error with key_comp or value_comp" << std::endl;
        else if (id == 0)
            std::cout << " [OK] key_comp(1) and value_comp(1)" << std::endl;
    }
}
#include <iostream>
#include <map>
#include "tests.h"
#include "verify.h"

/// The maps we will use for our tests
std::map<int, int>* operations_map = NULL;

void operation_tests(int id)
{
    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing map operations: find(2), count(1), "
               "lower_bound(2), upper_bound(), equal_range(2)\n");

    // test find(1a)
    global_barrier->arrive(id);
    {
        map_verifier v;
        bool ok = true;
        BEGIN_TX;
        operations_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        auto i = operations_map->find(2);
        ok &= i->second == 2;
        delete(operations_map);
        END_TX;
        if (!ok)
            std::cout << "["<<id<<"] error with find(1a)" << std::endl;
        else if (id == 0)
            std::cout << " [OK] find(1a)" << std::endl;
    }

    // test find(1b) with const iterator
    global_barrier->arrive(id);
    {
        map_verifier v;
        bool ok = true;
        BEGIN_TX;
        operations_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        const std::map<int, int>* c = operations_map;
        auto i = c->find(2);
        ok &= i->second == 2;
        delete(operations_map);
        END_TX;
        if (!ok)
            std::cout << "["<<id<<"] error with find(1a)" << std::endl;
        else if (id == 0)
            std::cout << " [OK] find(1b)" << std::endl;
    }

    // test count(1)
    global_barrier->arrive(id);
    {
        map_verifier v;
        int size, size2;
        BEGIN_TX;
        operations_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        size = operations_map->count(1);
        size2 = operations_map->count(9);
        delete(operations_map);
        END_TX;
        if (size != 1 || size2 != 0)
            std::cout << "["<<id<<"] error with count(1)" << std::endl;
        else if (id == 0)
            std::cout << " [OK] count(1)" << std::endl;
    }

    // test lower_bound(1a) and upper_bound(1a)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        operations_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        auto l = operations_map->lower_bound(2);
        auto u = operations_map->upper_bound(7);
        operations_map->erase(l, u);
        v.insert_all(operations_map);
        delete(operations_map);
        END_TX;
        v.check(" lower_bound(1a) and upper_bound(1a)", id, 4, {1, 1, 8, 8});
    }

    // test lower_bound(1b) and upper_bound(1b)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        operations_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        const std::map<int, int>* c = operations_map;
        auto l = c->lower_bound(2);
        auto u = c->upper_bound(7);
        operations_map->erase(l, u);
        v.insert_all(operations_map);
        delete(operations_map);
        END_TX;
        v.check(" lower_bound(1b) and upper_bound(1b)", id, 4, {1, 1, 8, 8});
    }

    // test equal_range(1a, 1b)
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        operations_map = new std::map<int, int>({{1, 1}, {2, 2}, {4, 4}, {8, 8}});
        const std::map<int, int>* c = operations_map;
        auto nt = operations_map->equal_range(2);
        operations_map->erase(nt.first, nt.second);
        auto ct = c->equal_range(8);
        operations_map->erase(ct.first, ct.second);
        v.insert_all(operations_map);
        delete(operations_map);
        END_TX;
        v.check(" equal_range(1a, 1b)", id, 4, {1, 1, 4, 4});
    }
}
#include <iostream>
#include <map>
#include "tests.h"
#include "verify.h"

std::map<int, int>* overload_map = NULL;

void overload_tests(int id)
{

    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing map relational operators ==, !=, <, >, <=, >=\n");

    // Test all operators in a single transaction
    global_barrier->arrive(id);
    {
        bool t1, t2, t3, t4, t5, t6;
        BEGIN_TX;
        overload_map = new std::map<int, int>({{1, 1}, {2, 2}, {3, 3}});
        std::map<int, int> l2({{1, 1}, {2, 2}, {3, 3}});
        std::map<int, int> l3({{2, 2}, {3, 3}, {4, 4}});
        t1 = *overload_map == l2;
        t2 = l2 != l3;
        t3 = l2 < l3;
        t4 = l3 > l2;
        t5 = *overload_map >= l2;
        t6 = *overload_map <= l2;
        delete overload_map;
        END_TX;
        if (!(t1 && t2 && t3 && t4 && t5 && t6))
            std::cout << "["<<id<<"] error on relational tests" << std::endl;
        else if (id == 0)
            std::cout << " [OK] relational tests" << std::endl;
    }

    global_barrier->arrive(id);
    if (id == 0)
        printf("Testing map swap\n");

    // Test swap
    global_barrier->arrive(id);
    {
        map_verifier v;
        BEGIN_TX;
        overload_map = new std::map<int, int>({{1, 1}, {2, 2}, {3, 3}, {4, 4}, {5, 5}});
        std::map<int, int> d2({{7, 7}, {8, 8}, {9, 9}});
        swap(*overload_map, d2);
        v.insert_all(overload_map);
        delete(overload_map);
        END_TX;
        v.check("std::swap (1)", id, 6, {7, 7, 8, 8, 9, 9});
    }
}

