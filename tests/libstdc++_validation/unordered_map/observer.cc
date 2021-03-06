#include <iostream>
#include <string>
#include <unordered_map>
#include <cassert>
#include "tests.h"
#include "verify.h"

/// The map we will use for our tests
typedef std::unordered_map<int, int> intmap;
typedef std::pair<int, int>          intpair;
typedef map_verifier verifier;

static intmap* observer_map = NULL;

void observer_tests(int id)
{
    // test hash function
    global_barrier->arrive(id);
    {
        intmap::hasher v;
        BEGIN_TX;
        observer_map = new intmap();
        v = observer_map->hash_function();
        delete(observer_map);
        observer_map = NULL;
        END_TX;
        std::cout << "  [OK] hash_function(1) = " << v(1) << std::endl;
    }

    // test key eq
    global_barrier->arrive(id);
    {
        intmap::key_equal v;
        BEGIN_TX;
        observer_map = new intmap();
        v = observer_map->key_eq();
        delete(observer_map);
        observer_map = NULL;
        END_TX;
        std::cout << "  [OK] key_eq(1, 1) = " << v(1, 1) << std::endl;
    }

    // test get allocator
    global_barrier->arrive(id);
    {
        verifier v;
        int size;
        BEGIN_TX;
        observer_map = new intmap();
        auto a = observer_map->get_allocator();
        size = observer_map->size();
        delete(observer_map);
        observer_map = NULL;
        END_TX;
        v.check_size("get_allocator (1)", id, size);
    }
}
