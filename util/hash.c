/*****************************************************************************
 * Licensed to Qualys, Inc. (QUALYS) under one or more
 * contributor license agreements.  See the NOTICE file distributed with
 * this work for additional information regarding copyright ownership.
 * QUALYS licenses this file to You under the Apache License, Version 2.0
 * (the "License"); you may not use this file except in compliance with
 * the License.  You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *****************************************************************************/

/**
 * @file
 * @brief IronBee &mdash; Hash Utility Functions Implementation
 * @author Brian Rectanus <brectanus@qualys.com>
 * @author Pablo Rincon <pablo.rincon.crespo@gmail.com>
 * @author Christopher Alfeld <calfeld@qualys.com>
 */

#include "ironbee_config_auto.h"

#include <ironbee/hash.h>

#include <ironbee/debug.h>
#include <ironbee/mpool.h>

#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

/* Internal Declarations */

/**
 * @defgroup IronBeeHashInternal Hash Internal
 * @ingroup IronBeeHash
 * @internal
 *
 * @{
 */

/**
 * Default size to use for ib_hash_create().
 * @internal
 *
 * Must be a power of 2.
 **/
#define IB_HASH_INITIAL_SIZE 16

/**
 * See ib_hash_entry_t()
 */
typedef struct ib_hash_entry_t    ib_hash_entry_t;

/**
 * See ib_hash_iterator_t()
 */
typedef struct ib_hash_iterator_t ib_hash_iterator_t;

/**
 * Entry in a ib_hash_t.
 * @internal
 **/
struct ib_hash_entry_t {
    /** Key. */
    const void          *key;
    /** Length of @c key. */
    size_t               key_length;
    /** Value. */
    void                *value;
    /** Hash of @c key. */
    uint32_t             hash_value;
    /** Next entry in slot for @c hash. */
    ib_hash_entry_t     *next_entry;
};

/**
 * External iterator for ib_hash_t.
 * @internal
 *
 * The end of the sequence is indicated by @c current_entry being NULL.
 * Any iterator is invalidated by any mutating operation on the hash.
 **/
struct ib_hash_iterator_t {
    /** Hash table we are iterating through. */
    const ib_hash_t     *hash;
    /** Current entry. */
    ib_hash_entry_t     *current_entry;
    /** Next entry. */
    ib_hash_entry_t     *next_entry;
    /** Which slot we are in. */
    size_t               slot_index;
};

/**
 * See ib_hash_t()
 **/
struct ib_hash_t {
    /** Hash function. */
    ib_hash_function_t   hash_function;
    /** Key equality predicate. */
    ib_hash_equal_t      equal_predicate;
    /**
     * Slots.
     *
     * Each slot holds a (possibly empty) linked list of ib_hash_entry_t's,
     * all of which have the same hash value.
     **/
    ib_hash_entry_t    **slots;
    /** Maximum slot index. */
    size_t               max_slot;
    /** Memory pool. */
    ib_mpool_t          *pool;
    /** Linked list of removed hash entries to use for recycling. */
    ib_hash_entry_t     *free;
    /** Number of entries. */
    size_t               size;
    /** Randomizer value. */
    uint32_t             randomizer;
};

/**
 * Search for an entry in @a hash matching @key.
 * @internal
 *
 * @param[in]  hash       Hash table.
 * @param[out] hash_entry Hash entry.
 * @param[in]  key        Key.
 * @param[in]  key_length Length of @a key.
 *
 * @returns
 * - IB_OK on success.
 * - IB_ENOENT if @a key not found.
 */
static ib_status_t ib_hash_find_entry(
    const ib_hash_t  *hash,
    ib_hash_entry_t **hash_entry,
    const void       *key,
    size_t            key_length
);

/**
 * Search for a hash entry in the list of entries starting at @a first.
 * @internal
 *
 * @param[in] hash       Hash table.
 * @param[in] first      Beginning of list to search.
 * @param[in] key        Key to search for.
 * @param[in] key_length Length of @a key.
 * @param[in] hash_value Hash value of @a key.
 *
 * @returns Hash entry if found and NULL otherwise.
 */
static ib_hash_entry_t *ib_hash_find_htentry(
     const ib_hash_t *hash,
     ib_hash_entry_t *first,
     const void      *key,
     size_t           key_length,
     uint32_t         hash_value
);

/**
 * Return iterator pointing to first entry of @a hash.
 * @internal
 *
 * @sa IB_HASH_LOOP()
 *
 * @param[in]  hash Hash table to iterate over.
 *
 * @return Iterator pointing to first entry in @a hash.
 */
static ib_hash_iterator_t ib_hash_first(
     const ib_hash_t* hash
);

/**
 * Move @a iterator to the next entry.
 * @internal
 *
 * @sa IB_HASH_LOOP()
 *
 * @param[in,out] iterator Iterator to advance.
 */
static void ib_hash_next(
    ib_hash_iterator_t *iterator
);

/**
 * Set @a entry to every entry in @a hash in sequence.
 *
 * @code
 * ib_hash_entry_t *current_entry;
 * IB_HASH_LOOP(current_entry, hash) {
 *   ...
 * }
 * @endcode
 *
 * @param[in,out] entry Set to each entry in @a hash in sequence.
 * @param[in]     hash  Hash table to iterate through.
 **/
#define IB_HASH_LOOP(entry,hash) \
    for ( \
        ib_hash_iterator_t iterator = ib_hash_first(hash); \
        ((entry) = iterator.current_entry) != NULL; \
        ib_hash_next(&iterator) \
    )

/**
 * Resize the number of slots in @a hash.
 * @internal
 *
 * @returns
 * - IB_OK on success.
 * - IB_EALLOC on allocation failure.
 */
static ib_status_t ib_hash_resize_slots(
    ib_hash_t *hash
);

/* End Internal Declarations */

/* Internal Definitions */

ib_hash_entry_t *ib_hash_find_htentry(
    const ib_hash_t *hash,
    ib_hash_entry_t *first,
    const void      *key,
    size_t           key_length,
    uint32_t         hash_value
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);
    assert(key  != NULL);

    for (
        ib_hash_entry_t* current_entry = first;
        current_entry != NULL;
        current_entry = current_entry->next_entry
    )
    {
        if (
            current_entry->hash_value == hash_value &&
            hash->equal_predicate(
                key,                key_length,
                current_entry->key, current_entry->key_length
            )
        ) {
            IB_FTRACE_RET_PTR(ib_hash_entry_t, current_entry);
        }
    }
    IB_FTRACE_RET_PTR(ib_hash_entry_t, NULL);
}

ib_status_t ib_hash_find_entry(
     const ib_hash_t  *hash,
     ib_hash_entry_t **hash_entry,
     const void       *key,
     size_t            key_length
) {
    IB_FTRACE_INIT();

    assert(hash_entry != NULL);
    assert(hash       != NULL);
    assert(key        != NULL);

    ib_hash_entry_t *current_slot  = NULL;
    ib_hash_entry_t *current_entry = NULL;
    uint32_t         hash_value    = 0;
    if (hash_entry == NULL || hash == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    hash_value = hash->hash_function(key, key_length, hash->randomizer);

    /* hash->max_slot+1 is a power of 2 */
    current_slot = hash->slots[hash_value & hash->max_slot];
    current_entry = ib_hash_find_htentry(
        hash,
        current_slot,
        key,
        key_length,
        hash_value
    );
    if (current_entry == NULL) {
        *hash_entry = NULL;
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }
    *hash_entry = current_entry;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_hash_iterator_t ib_hash_first(
    const ib_hash_t *hash
) {
    /* There is no ftrace return macro for custom types. */
    assert(hash != NULL);

    ib_hash_iterator_t iterator;

    memset(&iterator, 0, sizeof(iterator));
    iterator.hash = hash;
    ib_hash_next(&iterator);

    return iterator;
}

void ib_hash_next(
    ib_hash_iterator_t *iterator
) {
    IB_FTRACE_INIT();

    assert(iterator != NULL);

    iterator->current_entry = iterator->next_entry;
    while (! iterator->current_entry) {
        if (iterator->slot_index > iterator->hash->max_slot) {
            IB_FTRACE_RET_VOID();
        }
        iterator->current_entry = iterator->hash->slots[iterator->slot_index];
        ++iterator->slot_index;
    }
    iterator->next_entry = iterator->current_entry->next_entry;

    IB_FTRACE_RET_VOID();
}

ib_status_t ib_hash_resize_slots(
    ib_hash_t *hash
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);

    ib_hash_entry_t **new_slots     = NULL;
    ib_hash_entry_t  *current_entry = NULL;
    size_t            new_max_slot = 0;

    /* Maintain power of 2 slots */
    new_max_slot = 2 * hash->max_slot + 1;
    new_slots = (ib_hash_entry_t **)ib_mpool_calloc(
        hash->pool,
        new_max_slot + 1,
        sizeof(*new_slots)
    );
    if (new_slots == NULL) {
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    IB_HASH_LOOP(current_entry, hash) {
        size_t i                  = current_entry->hash_value & new_max_slot;
        current_entry->next_entry = new_slots[i];
        new_slots[i]              = current_entry;
    }
    hash->max_slot = new_max_slot;
    hash->slots     = new_slots;

    IB_FTRACE_RET_STATUS(IB_OK);
}

/* End Internal Definitions */

uint32_t ib_hashfunc_djb2(
    const void *key,
    size_t      key_length,
    uint32_t    randomizer
) {
    IB_FTRACE_INIT();

    assert(key != NULL);

    uint32_t      hash  = randomizer;
    const char   *key_s = (const char *)key;

    for (size_t i = 0; i < key_length; ++i) {
        hash = ((hash << 5) + hash) + key_s[i];
    }

    IB_FTRACE_RET_UINT(hash);
}

uint32_t DLL_PUBLIC ib_hashfunc_djb2_nocase(
    const void *key,
    size_t      key_length,
    uint32_t    randomizer
) {
    IB_FTRACE_INIT();

    assert(key != NULL);

    uint32_t             hash  = randomizer;
    const unsigned char *key_s = (const unsigned char *)key;

    for (size_t i = 0; i < key_length; ++i) {
        hash = ((hash << 5) + hash) + tolower(key_s[i]);
    }

    IB_FTRACE_RET_UINT(hash);
}

int DLL_PUBLIC ib_hashequal_default(
    const void *a,
    size_t      a_length,
    const void *b,
    size_t      b_length
) {
    IB_FTRACE_INIT();

    assert(a != NULL);
    assert(b != NULL);

    IB_FTRACE_RET_INT(
        (a_length == b_length) &&
        (memcmp(a, b, a_length) == 0)
   );
}

int DLL_PUBLIC ib_hashequal_nocase(
    const void *a,
    size_t      a_length,
    const void *b,
    size_t      b_length
) {
    IB_FTRACE_INIT();

    assert(a != NULL);
    assert(b != NULL);

    const unsigned char *a_s = (const unsigned char *)a;
    const unsigned char *b_s = (const unsigned char *)b;

    if (a_length != b_length) {
        IB_FTRACE_RET_INT(0);
    }

    for (size_t i = 0; i < a_length; ++i) {
        if (tolower(a_s[i]) != tolower(b_s[i])) {
            IB_FTRACE_RET_INT(0);
        }
    }

    IB_FTRACE_RET_INT(1);
}

ib_status_t ib_hash_create_ex(
    ib_hash_t          **hash,
    ib_mpool_t          *pool,
    size_t               size,
    ib_hash_function_t   hash_function,
    ib_hash_equal_t      equal_predicate
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);
    assert(pool != NULL);
    assert(size > 0);

    ib_hash_t *new_hash = NULL;

    if (hash == NULL) {
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    {
        int num_ones = 0;
        for (
            int temp_size = size;
            temp_size > 0;
            temp_size = temp_size >> 1
        ) {
            if ( ( temp_size & 1 ) == 1 ) {
                ++num_ones;
            }
        }
        if (num_ones != 1) {
            IB_FTRACE_RET_STATUS(IB_EINVAL);
        }
    }

    new_hash = (ib_hash_t *)ib_mpool_alloc(pool, sizeof(*new_hash));
    if (new_hash == NULL) {
        *hash = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    ib_hash_entry_t **slots = (ib_hash_entry_t **)ib_mpool_calloc(
        pool,
        size + 1,
        sizeof(*slots)
    );
    if (slots == NULL) {
        *hash = NULL;
        IB_FTRACE_RET_STATUS(IB_EALLOC);
    }

    new_hash->hash_function   = hash_function;
    new_hash->equal_predicate = equal_predicate;
    new_hash->max_slot        = size-1;
    new_hash->slots           = slots;
    new_hash->pool            = pool;
    new_hash->free            = NULL;
    new_hash->size            = 0;
    new_hash->randomizer      = (uint32_t)clock();

    *hash = new_hash;

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_hash_create(
    ib_hash_t  **hash,
    ib_mpool_t  *pool
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);
    assert(pool != NULL);

    IB_FTRACE_RET_STATUS(ib_hash_create_ex(
        hash,
        pool,
        IB_HASH_INITIAL_SIZE,
        ib_hashfunc_djb2,
        ib_hashequal_default
    ));
}

ib_status_t DLL_PUBLIC ib_hash_create_nocase(
    ib_hash_t  **hash,
    ib_mpool_t  *pool
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);
    assert(pool != NULL);

    IB_FTRACE_RET_STATUS(ib_hash_create_ex(
        hash,
        pool,
        IB_HASH_INITIAL_SIZE,
        ib_hashfunc_djb2_nocase,
        ib_hashequal_nocase
    ));
}

ib_mpool_t DLL_PUBLIC *ib_hash_pool(
    ib_hash_t *hash
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);

    IB_FTRACE_RET_PTR(ib_mpool_t, hash->pool);
}

size_t DLL_PUBLIC ib_hash_size(
    ib_hash_t* hash
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);

    IB_FTRACE_RET_UINT(hash->size);
}

ib_status_t ib_hash_get_ex(
    const ib_hash_t  *hash,
    void             *value,
    const void       *key,
    size_t            key_length
) {
    IB_FTRACE_INIT();

    assert(value != NULL);
    assert(hash  != NULL);

    ib_status_t      rc;
    ib_hash_entry_t *current_entry = NULL;

    if (key == NULL) {
        *(void **)value = NULL;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    rc = ib_hash_find_entry(
        hash,
        &current_entry,
        key,
        key_length
    );
    if (rc == IB_OK) {
        assert(current_entry != NULL);

        *(void **)value = current_entry->value;
    }
    else {
        *(void **)value = NULL;
    }

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_hash_get(
    const ib_hash_t   *hash,
    void              *value,
    const char        *key
) {
    IB_FTRACE_INIT();

    assert(value != NULL);
    assert(hash  != NULL);

    if (key == NULL) {
        *(void **)value = NULL;
        IB_FTRACE_RET_STATUS(IB_EINVAL);
    }

    IB_FTRACE_RET_STATUS(ib_hash_get_ex(
        hash,
        value,
        key,
        strlen(key)
    ));
}

ib_status_t ib_hash_get_all(
    const ib_hash_t *hash,
    ib_list_t       *list
) {
    IB_FTRACE_INIT();

    assert(list != NULL);
    assert(hash != NULL);

    ib_hash_entry_t* current_entry = NULL;

    IB_HASH_LOOP(current_entry, hash) {
        ib_list_push(list, current_entry->value);
    }

    if (ib_list_elements(list) <= 0) {
        IB_FTRACE_RET_STATUS(IB_ENOENT);
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_hash_set_ex(
    ib_hash_t  *hash,
    const void *key,
    size_t      key_length,
    void       *value
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);
    assert(key  != NULL);

    uint32_t     hash_value = 0;
    size_t       slot_index = 0;
    int          found      = 0;

    ib_hash_entry_t  *current_entry         = NULL;
    /* Points to pointer that points to current_entry */
    ib_hash_entry_t **current_entry_handle  = NULL;

    hash_value = hash->hash_function(key, key_length, hash->randomizer);
    slot_index = (hash_value & hash->max_slot);

    current_entry_handle = &hash->slots[slot_index];

    while (*current_entry_handle != NULL) {
        current_entry = *current_entry_handle;
        if (
            current_entry->hash_value == hash_value &&
            hash->equal_predicate(
               current_entry->key, current_entry->key_length,
               key,                key_length
            )
        ) {
            found = 1;
            break;
        }
        current_entry_handle = &(current_entry->next_entry);
    }
    /* current_entry is now the entry to change, and current_entry_handler
     * points to the pointer to it.
     */
    if (found) {
        assert(current_entry != NULL);
        assert(current_entry == *current_entry_handle);

        /* Update. */
        current_entry->value = value;

        /* Delete if appropriate. */
        if (value == NULL) {
            /* Delete */
            --hash->size;

            /* Remove from slot list. */
            *current_entry_handle = current_entry->next_entry;

            /* Add to free list. */
            current_entry->next_entry = hash->free;
            hash->free                = current_entry;
        }
    }
    else {
        /* It's not in the list. Add it if value != NULL. */
        if (value != NULL) {
            ib_hash_entry_t *entry = NULL;

            if (hash->free != NULL) {
                entry = hash->free;
                hash->free = entry->next_entry;
            }
            else {
                entry = (ib_hash_entry_t *)ib_mpool_alloc(
                    hash->pool,
                    sizeof(*entry)
                );
                if (entry == NULL) {
                    IB_FTRACE_RET_STATUS(IB_EALLOC);
                }
            }

            entry->hash_value = hash_value;
            entry->key        = key;
            entry->key_length = key_length;
            entry->value      = value;
            entry->next_entry = hash->slots[slot_index];

            hash->slots[slot_index] = entry;

            ++hash->size;

            /* If we have more elements that slots, resize. */
            if (hash->size > hash->max_slot+1) {
                IB_FTRACE_RET_STATUS(ib_hash_resize_slots(hash));
            }
        }
        /* Else value == NULL and no changes are needed. */
    }

    IB_FTRACE_RET_STATUS(IB_OK);
}

ib_status_t ib_hash_set(
    ib_hash_t  *hash,
    const char *key,
    void       *value
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);
    assert(key  != NULL);

    IB_FTRACE_RET_STATUS(ib_hash_set_ex(
        hash,
        (void *)key,
        strlen(key),
        value
    ));
}

void ib_hash_clear(ib_hash_t *hash) {
    IB_FTRACE_INIT();

    assert(hash != NULL);

    for (size_t i = 0; i <= hash->max_slot; ++i) {
        if (hash->slots[i] != NULL) {
            ib_hash_entry_t *current_entry;
            for (
                current_entry = hash->slots[i];
                current_entry->next_entry != NULL;
                current_entry = current_entry->next_entry
            ) {
                current_entry->value = NULL;
            }

            current_entry->next_entry = hash->free;
            hash->free                = hash->slots[i];
            hash->slots[i]            = NULL;
        }
    }
    hash->size = 0;

    IB_FTRACE_RET_VOID();
}

ib_status_t ib_hash_remove_ex(
    ib_hash_t *hash,
    void      *value,
    void      *key,
    size_t     key_length
) {
    IB_FTRACE_INIT();

    assert(hash  != NULL);
    assert(key   != NULL);

    ib_status_t  rc          = IB_ENOENT;
    void        *local_value = NULL;

    rc = ib_hash_get_ex(hash, &local_value, key, key_length);
    if (rc != IB_OK) {
        IB_FTRACE_RET_STATUS(rc);
    }

    if ((value != NULL) && (local_value != NULL)) {
        *(void **)value = local_value;
    }
    rc = ib_hash_set_ex(hash, key, key_length, NULL);

    IB_FTRACE_RET_STATUS(rc);
}

ib_status_t ib_hash_remove(
    ib_hash_t   *hash,
    void        *value,
    const char  *key
) {
    IB_FTRACE_INIT();

    assert(hash != NULL);
    assert(key  != NULL);

    IB_FTRACE_RET_STATUS(
        ib_hash_remove_ex(hash, value, (void *)key, strlen(key))
    );
}

/** @} IronBeeUtilHash */
