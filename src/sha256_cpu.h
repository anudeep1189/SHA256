/*
 * sha256_cpu.h - CPU Implementation of SHA256 Hashing (Host-Side)
 *
 * Declares the public API for batch SHA-256 hashing on CPU.
 * This implementation processes hashes in batches with identical nonce logic
 * to the GPU version (thread 0 = original, threads 1+ append unique 4-byte nonce).
 *
 * Based on the public domain Reference Implementation in C, by Brad Conte.
 * This file is released into the Public Domain.
 */

#pragma once

#include "config.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * mcm_cpu_sha256_hash_batch - Compute SHA256 hashes on CPU
 *
 * Parameters:
 *   in      - Input data buffer (shared by all hashes in batch)
 *   inlen   - Length of input data in bytes
 *   out     - Output buffer for hashes (must be at least n_batch * 32 bytes)
 *   n_batch - Number of hashes to compute (batch size)
 *
 * Returns:
 *   0       - Success
 *   non-zero - Error (e.g., out of memory)
 *
 * Hash Layout:
 *   - out[0*32 .. 0*32+31]   = SHA256(in) with no nonce (original input)
 *   - out[1*32 .. 1*32+31]   = SHA256(in || nonce[1]) with 4-byte big-endian nonce
 *   - out[2*32 .. 2*32+31]   = SHA256(in || nonce[2]) with 4-byte big-endian nonce
 *   - ...
 *   - out[(n_batch-1)*32 .. (n_batch-1)*32+31] = SHA256(in || nonce[n_batch-1])
 *
 * Nonce Format (for thread i > 0):
 *   nonce[0] = (i >> 24) & 0xFF
 *   nonce[1] = (i >> 16) & 0xFF
 *   nonce[2] = (i >> 8) & 0xFF
 *   nonce[3] = i & 0xFF
 */
int mcm_cpu_sha256_hash_batch(const BYTE* in, SHA_WORD inlen, BYTE* out, SHA_WORD n_batch);

#ifdef __cplusplus
}
#endif

