//
// Created by philipp on 11.07.18.
//

#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "../../include/common/Dict.h"

/**@brief Calculates the hash value of a memory block relative to hash value of
 * the previous block.
 * @param key   pointer to the memory block
 * @param size  size of the memory block
 * @param hval  hash value of the previous memory block
 */
uint64_t hash_cont(const void * key, size_t size, uint64_t hval)
{
	const char * ptr = key;

	while (size-- > 0)
	{
		hval ^= *ptr;
		hval *= 0x100000001b3ull;
		++ptr;
	}

	return hval;
}

/**@brief Calculates the hash value of a first memory block.
 * @param key   pointer to the memory block
 * @param size  size of the memory block
 */
inline uint64_t hash(const void * key, size_t size)
{
	return hash_cont(key, size, 0xcbf29ce484222325ull);
}

int dictInit(dict_t * self, size_t keySize, dictKeyDestroyer_t destroyer, dictKeyComparator_t comparator)
{
	self->bits = 3;
	self->left = 1u << self->bits;
	self->keySize = keySize;
	self->destroyer = destroyer;
	self->comparator = comparator;
	self->data = calloc(self->left, sizeof(*self->data));
	return self->data == NULL;
}

void dictRelease(dict_t * self)
{
	dict_entry_t * it = self->data;
	dict_entry_t * end = it + (1u << self->bits);

	while (it < end)
	{
		if (it->val != NULL)
			free(it->key);
		++it;
	}

	free(self->data);
}

void dictInsert(dict_t * self, const void * key, void * val)
{
	unsigned int loc;

	if (self->left == 0)
		grow(self);

	if (!locate(self, key, &loc))
	{
		self->data[loc].key = malloc(self->keySize);
		memcpy(self->data[loc].key, key, self->keySize);
		--self->left;
	}

	self->data[loc].val = val;
}

void * dictFind(const dict_t * self, const void * key)
{
	unsigned int loc;
	return locate(self, key, &loc)
	       ? self->data[loc].val : NULL;
}

void dictRemove(dict_t * self, const void * key)
{
	unsigned int loc;

	if (locate(self, key, &loc))
	{
		self->destroyer(self->data[loc].key);
		free(self->data[loc].key);
		self->data[loc].val = NULL;
		++self->left;
	}
}

void grow(dict_t * self)
{
	dict_entry_t * data = self->data, * it, * end;
	unsigned int cap = 1u << self->bits;
	unsigned int loc;

	self->data = calloc(2 * cap, sizeof(*self->data));
	++self->bits;
	self->left += cap;

	for (it = data, end = it + cap; it < end; ++it)
	{
		if (it->val != NULL)
		{
			locate(self, it->key, &loc);
			self->data[loc] = *it;
		}
	}

	free(data);
}

int locate(const dict_t * self, const void * key, unsigned int * result)
{
	enum
	{
		EMPTY, CLEAR, MATCH
	} state = EMPTY;
	uint64_t hval = hash(key, self->keySize);
	const unsigned int mask = (1u << self->bits) - 1;
	const unsigned int initial = hval & mask;
	unsigned int probe = initial;

	hval = (hval >> (self->bits - 1)) | 1;

	do
	{
		if (self->data[probe].key == NULL)
		{
			if (state == EMPTY)
				*result = probe;
			break;
		}
		else if (self->data[probe].val == NULL)
		{
			if (state == EMPTY)
				state = CLEAR, *result = probe;
		}
		else if (self->comparator(self->data[probe].key, key))
		{
			state = MATCH, *result = probe;
			break;
		}
	} while ((probe = (probe + hval) & mask) != initial);
	return state == MATCH;
}