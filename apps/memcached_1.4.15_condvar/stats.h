/* stats */
void stats_prefix_init(void);
// [branch 004] This function is called from a relaxed transaction
// [branch 005] This function is actually transaction safe
__attribute__((transaction_safe))
void stats_prefix_clear(void);
void stats_prefix_record_get(const char *key, const size_t nkey, const bool is_hit);
void stats_prefix_record_delete(const char *key, const size_t nkey);
void stats_prefix_record_set(const char *key, const size_t nkey);
/*@null@*/
char *stats_prefix_dump(int *length);
