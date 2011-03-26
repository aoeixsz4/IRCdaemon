/* hash.h - header file for hash table implementation */
#ifndef __HASH_H__
#define __HASH_H__

#define PERM_TABLE_SZ 256
#define TABLE_SZ 256

struct hash_bucket {
	char *key;
	void *value; /* pointer to whatever */
	void *next; /* this will be a pointer to another struct_bucket */
};

struct hash_table {
	unsigned int nr_entries;
	struct hash_bucket *bucket_array[TABLE_SZ];
	char permut_table[PERM_TABLE_SZ];
};

void hash_init(struct hash_table *);
int hash_insert(struct hash_table *, const char *key, void *value);
void *hash_lookup(struct hash_table *, const char *key);
void *hash_remove(struct hash_table *, const char *key);
void hash_free(struct hash_table *); /* NB this just delinks and frees all buckets */
#define hash_assert(table_ptr) \
		assert((table_ptr)->nr_entries == 0)
char **hash_keys(struct hash_table *);
void **hash_values(struct hash_table *);
struct hash_bucket **hash_buckets(struct hash_table *);

#endif /* __HASH_H__ */
