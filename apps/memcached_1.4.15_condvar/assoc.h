/* associative array */
void assoc_init(const int hashpower_init);
// [branch 004b] This function is called from a relaxed transaction
// [branch 009b] With safe memcmp, this becomes safe
__attribute__((transaction_safe))
item *assoc_find(const char *key, const size_t nkey, const uint32_t hv);
// [branch 004] This function is called from a relaxed transaction
// [branch 012] With oncommit, this becomes safe
__attribute__((transaction_safe))
int assoc_insert(item *item, const uint32_t hv);
// [branch 004] This function is called from a relaxed transaction
// [branch 009] With safe memcmp, this becomes safe
__attribute__((transaction_safe))
void assoc_delete(const char *key, const size_t nkey, const uint32_t hv);
void do_assoc_move_next_bucket(void);
int start_assoc_maintenance_thread(void);
void stop_assoc_maintenance_thread(void);
extern unsigned int hashpower;
