#include "tm_hacks.h"

std::vector<query_t*>* make_vector_query(int num)
{
    auto res = new std::vector<query_t*>();
    if (num > 0)
        res->reserve(num);
    return res;
}

void vector_query_push(std::vector<query_t*>* v, query_t* q)
{
    v->push_back(q);
}

