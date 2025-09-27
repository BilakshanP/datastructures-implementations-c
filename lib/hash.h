#ifndef HASH_H
#define HASH_H

#include <stddef.h>
#include <stdint.h>

// Warning: This implementation is not thread-safe, and is for educational purposes only.

/// hashmap_sip returns a hash value for `data` using SipHash-2-4. Takes a key
/// of size `128 bits`. i.e `uint64_t seed_0 + uint64_t seed_1 ~ uint128_t`.
uint64_t hash_sip(const void *data, size_t len, uint64_t seed0, uint64_t seed1);

/// hashmap_murmur returns a hash value for `data` using Murmur3_86_128.
uint64_t hash_murmur(const void *data, size_t len, uint64_t seed);

/// hashmap_murmur returns a hash value for `data` using xxHash3.
uint64_t hash_xxhash3(const void *data, size_t len, uint64_t seed);

/// dhb2_hash, used for strings (`NULL` terminated)
uint64_t djb2_hash(char *str);

/// fnv-1a
uint64_t fnv1a(const void *key, size_t len);

#endif  // HASH_H