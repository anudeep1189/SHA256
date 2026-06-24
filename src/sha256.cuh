/*
 * sha256.cuh CUDA Implementation of SHA256 Hashing    
 *
 * Date: 12 June 2019
 * Revision: 1
 * 
 * Based on the public domain Reference Implementation in C, by
 * Brad Conte, original code here:
 *
 * https://github.com/B-Con/crypto-algorithms
 *
 * This file is released into the Public Domain.
 */


#pragma once
#include "config.h"
#include <cuda_runtime_api.h>
cudaError_t mcm_cuda_sha256_hash_batch(BYTE* in, SHA_WORD inlen, BYTE* out, SHA_WORD n_batch);
