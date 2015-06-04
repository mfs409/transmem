#include "tm_hacks.h"

bool custom_map_insertion(std::map<edge_t*, element_t*, element_mapCompareEdge_t>* map,
                          edge_t* edge,
                          element_t* element)
{
    return map->insert(std::make_pair(edge, element)).second;
}

bool custom_set_insertion(std::set<element_t*, element_listCompare_t>* set,
                          element_t* element)
{
    return set->insert(element).second;
}

bool custom_set_insertion(std::set<edge_t*, element_listCompareEdge_t>* set,
                          edge_t* edge)
{
    return set->insert(edge).second;
}

bool custom_set_insertion(std::multiset<element_t*, element_heapCompare_t>* set,
                          element_t* element)
{
    set->insert(element);
    return true;
}

