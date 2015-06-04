#pragma once

#include <vector>
#include "query.h"

__attribute__((transaction_safe))
std::vector<query_t*>* make_vector_query(int num = 0);

__attribute__((transaction_safe))
void vector_query_push(std::vector<query_t*>* v, query_t* q);
