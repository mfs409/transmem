/* See items.c */
// [branch 004] This function is called from a relaxed transaction
// [branch 005] This function is actually transaction safe
__attribute__((transaction_safe))
uint64_t get_cas_id(void);

/*@null@*/
// [branch 004b] This function is called from a relaxed transaction
// [branch 012b] With fprintf oncommit, this becomes safe
__attribute__((transaction_safe))
item *do_item_alloc(char *key, const size_t nkey, const int flags, const rel_time_t exptime, const int nbytes, const uint32_t cur_hv);
// [branch 004] This function is called from a relaxed transaction
// [branch 008] With safe asserts, this becomes safe
__attribute__((transaction_safe))
void item_free(item *it);
bool item_size_ok(const size_t nkey, const int flags, const int nbytes);

// [branch 004b] This function is called from a relaxed transaction
// [branch 012b] With fprintf oncommit, this becomes safe
__attribute__((transaction_safe))
int  do_item_link(item *it, const uint32_t hv);     /** may fail if transgresses limits */
// [branch 004b] This function is called from a relaxed transaction
// [branch 009b] This becomes safe with safe memcmp
__attribute__((transaction_safe))
void do_item_unlink(item *it, const uint32_t hv);
// [branch 004] This function is called from a relaxed transaction
// [branch 009] With safe memcmp, this becomes safe
__attribute__((transaction_safe))
void do_item_unlink_nolock(item *it, const uint32_t hv);
// [branch 004] This function is called from a relaxed transaction
// [branch 008] With safe asserts, this becomes safe
__attribute__((transaction_safe))
void do_item_remove(item *it);
// [branch 004b] This function is called from a relaxed transaction
// [branch 008b] With safe asserts, this becomes safe
__attribute__((transaction_safe))
void do_item_update(item *it);   /** update LRU time to current and reposition */
// [branch 004b] This function is called from a relaxed transaction
// [branch 012b] With fprintf oncommit, this becomes safe
__attribute__((transaction_safe))
int  do_item_replace(item *it, item *new_it, const uint32_t hv);

/*@null@*/
// [branch 004] This function is called from a relaxed transaction
// [branch 011] With safe snprintf, this becomes safe
__attribute__((transaction_safe))
char *do_item_cachedump(const unsigned int slabs_clsid, const unsigned int limit, unsigned int *bytes);
// [branch 004] This function is called from a relaxed transaction
// [branch 011] With safe stats, this becomes safe
__attribute__((transaction_safe))
void do_item_stats(ADD_STAT add_stats, void *c);
// [branch 004] This function is called from a relaxed transaction
// [branch 011] With safe stats, this becomes safe
__attribute__((transaction_safe))
void do_item_stats_totals(ADD_STAT add_stats, void *c);
/*@null@*/
// [branch 004] This function is called from a relaxed transaction
// [branch 011] With safe stats, this becomes safe
__attribute__((transaction_safe))
void do_item_stats_sizes(ADD_STAT add_stats, void *c);
// [branch 004] This function is called from a relaxed transaction
// [branch 009] With safe memcmp, this becomes safe
__attribute__((transaction_safe))
void do_item_flush_expired(void);

// [branch 004b] This function is called from a relaxed transaction
// [branch 012b] With fprintf oncommit, this becomes safe
__attribute__((transaction_safe))
item *do_item_get(const char *key, const size_t nkey, const uint32_t hv);
// [branch 004b] This function is called from a relaxed transaction
// [branch 012b] With fprintf oncommit, this becomes safe
__attribute__((transaction_safe))
item *do_item_touch(const char *key, const size_t nkey, uint32_t exptime, const uint32_t hv);
void item_stats_reset(void);
// [branch 002] Removed declaration of cache lock
void item_stats_evictions(uint64_t *evicted);
