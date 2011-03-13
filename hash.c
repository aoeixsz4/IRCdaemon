#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include "hash.h"

/* make all hash keys case insensitive by copying the key to
 * dynamic memory, then making any lowercase letters uppercase */
static char *
_ncasedup (const char *str)
{
	char *new;
	int i;
	new = strdup(str);
	for (i = 0; new && new[i]; ++i)
	{
		if (new[i] >= 'a' && new[i] <= 'z')
			new[i] -= 'a' - 'A';
	}
	return new;
}

/* shamelessly stolen from http://en.wikipedia.org/wiki/Pearson_hash */
static unsigned char
_pearson_hash (struct hash_table *table, const char *key)
{
	unsigned char hash, index, *tmp;
	hash = 0;
	for (tmp = (unsigned char *)key; *tmp; tmp++)
	{
		index = hash ^ *tmp;
		hash = table->permut_table[index];
	}
	return hash;
}

void
hash_init (struct hash_table *table)
{
	int i, j, randint;
	memset (table, 0, sizeof(*table));
	srandom(time(NULL));
	for (i = 0; i < PERM_TABLE_SZ; i++)
	{
		randint = random();
		for (j = 0; j < 4 && i < PERM_TABLE_SZ; j++)
		{
			table->permut_table[i] = (char)(randint & 0xff);
			randint >>= 8;
			i++;
		}
	}
}

/* table must be initialised by hash_init() already
 * we dynamically allocate a new bucket entry,
 * with a pointer to a *copy* of the key, also dynamic
 * success: return 0, otherwise return -1
 * NB: its up to the callers to ensure there are no duplicate
 *     keys. why should we have to do it here? Besides,
 *     duplicate keys doesnt stop the hash table functioning
 *     the duplicates simply behave as a FILO stack type thing
 */
int
hash_insert (struct hash_table *table, const char *key, void *value)
{
	struct hash_bucket *bucket;
	unsigned char hash;
	bucket = malloc(sizeof(*bucket));
	if (!bucket) return -1;
	bucket->key = _ncasedup(key);
	if (!bucket->key)
	{
		free(bucket);
		return -1;
	}
	bucket->value = value;
	bucket->next = NULL;
	hash = _pearson_hash (table, key);
	/* deal with collision case */
	if (table->bucket_array[hash])
		bucket->next = table->bucket_array[hash];
	table->bucket_array[hash] = bucket;
	table->nr_entries++;
	return 0;
}

/* look up a key in the hash table and return its bucket
 * makes sense to have a seperate function here as both
 * hash_lookup() and hash_remove() require this functionality
 * the latter needs args prev and hash_p, the former may
 * pass these as NULL */
static struct hash_bucket *
_bucket_lookup (struct hash_table *table, const char *key,
				struct hash_bucket **prev, unsigned char *hash_p)
{
	unsigned char hash;
	struct hash_bucket *bucket;
	char *mykey;
	mykey = _ncasedup(key);
	hash = _pearson_hash(table, mykey);
	if (hash_p) *hash_p = hash;
	bucket = table->bucket_array[hash];
	/* does any key exist for that hash? */
	if (!bucket)
	{
		free(mykey);
		return NULL;
	}
	if (prev) *prev = NULL;
	while (bucket)
	{
		if (!strcmp(mykey, bucket->key)) return bucket;
		if (prev && bucket->next) *prev = bucket;
		bucket = bucket->next;
	}
	free(mykey);
	return NULL;
}

/* look up a key in the table and return a pointer to
 * its associated data */
void *
hash_lookup (struct hash_table *table, const char *key)
{
	struct hash_bucket *bucket;
	bucket = _bucket_lookup(table, key, NULL, NULL);
	if (!bucket) return NULL;
	return bucket->value;
}

/* look up a key in the table and remove it, freeing
 * any dynamic data allocated by our hash table implementation
 * also return the data pointer so the caller can free() that
 * if necessary. we don't want to do that here */
void *
hash_remove (struct hash_table *table, const char *key)
{
	struct hash_bucket *bucket, *prev;
	void *data;
	unsigned char hash;
	bucket = _bucket_lookup(table, key, &prev, &hash);
	if (!bucket) return NULL;
	/* having retrieved the bucket, we can unlink it
	 * by linking the previous bucket to the next bucket
	 * or if there is no previous bucket we simply
	 * point the hash to the next bucket
	 * this also works fine if bucket->next is NULL */
	if (prev) prev->next = bucket->next;
	else table->bucket_array[hash] = bucket->next;
	/* now we just have to free the dynamic memory */
	/* WOAH WAIT, save the data pointer first */
	data = bucket->value;
	free(bucket->key);
	free(bucket);
	table->nr_entries--;
	return data;
}

/* cleanup function, searches entire table, delinking
 * and freeing buckets */
void
hash_free (struct hash_table *table)
{
	int i;
	struct hash_bucket *bucket;
	i = 0;
	while (i < TABLE_SZ)
	{
		bucket = table->bucket_array[i];
		if (bucket)
		{
			table->bucket_array[i] = bucket->next;
			free(bucket->key);
			free(bucket);
		}
		else
		{
			i++;
		}
	}
}

/* char **hash_keys (table)
 * return a pointer to an array of pointers
 * to keys, the caller must be aware that these are
 * the actual keys in the table not copies */
char **
hash_keys (struct hash_table *table)
{
	struct hash_bucket *bucket;
	char **keys;
	int i, j;

	/* entries + 1 to provide a NULL terminal */
	keys = malloc((table->nr_entries + 1) * sizeof(*keys));
	if (!keys) return NULL;

	/* iterate thru array */
	i = j = 0;
	while (i < TABLE_SZ)
	{
		bucket = table->bucket_array[i];
		/* iterate thru linked list */
		while (bucket)
		{
			keys[j] = bucket->key;
			j++;
			bucket = bucket->next;
		}
	}

	/* NULL terminal */
	keys[j] = NULL;
	return keys;
}

/* void **hash_values (table)
 * return malloc() array of void * data pointers to users' data */
void **
hash_values (struct hash_table *table)
{
	struct hash_bucket *bucket;
	void **ptr_array;
	int i, j;

	/* entries + 1 to provide NULL terminal, users may have inserted NULL
	 * keys and if so, they can use table->nr_entries instead of our NULL */
	ptr_array = malloc((table->nr_entries + 1) * sizeof(*ptr_array));
	if (!ptr_array) return NULL;

	/* iterate thru array */
	i = j = 0;
	while (i < TABLE_SZ)
	{
		bucket = table->bucket_array[i];
		while (bucket)
		{
			ptr_array[j] = bucket->value;
			j++;
			bucket = bucket->next;
		}
	}

	/* NULL terminal */
	ptr_array[j] = NULL;
	return ptr_array;
}

/* struct hash_bucket **hash_buckets (table)
 * return null terminated malloc() array of pointers to buckets */
struct hash_bucket **
hash_buckets (struct hash_table *table)
{
	struct hash_bucket *bucket, **buckets;
	int i, j;

	/* entries + 1 for NULL terminal */
	buckets = malloc((table->nr_entries + 1) * sizeof(*buckets));
	if (!buckets) return NULL;

	/* unroll hash table */
	i = j = 0;
	while (i < TABLE_SZ)
	{
		bucket = table->bucket_array[i];
		while (bucket)
		{
			buckets[j] = bucket;
			j++;
			bucket = bucket->next;
		}
	}

	/* NULL terminal */
	buckets[j] = NULL;
	return buckets;
}
