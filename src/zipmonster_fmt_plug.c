/* This format is reverse engineered from InsidePro Hash Manager!
 *
 * This software is Copyright (c) 2016, Dhiru Kholia <dhiru.kholia at gmail.com>,
 * and it is hereby released to the general public under the following terms:
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted.
 */

#if FMT_EXTERNS_H
extern struct fmt_main fmt_zipmonster;
#elif FMT_REGISTERS_H
john_register_one(&fmt_zipmonster);
#else

#include "arch.h"
#include "sha.h"
#include "md5.h"
#include <string.h>
#include "misc.h"
#include "common.h"
#include "formats.h"
#include "params.h"
#include "options.h"
#ifdef _OPENMP
#include <omp.h>
#ifndef OMP_SCALE
#define OMP_SCALE               8
#endif
#endif
#include "memdbg.h"

#define FORMAT_LABEL            "ZipMonster"
#define FORMAT_NAME             "MD5(ZipMonster)"
#define ALGORITHM_NAME          "MD5 x 50000"
#define BENCHMARK_COMMENT       ""
#define BENCHMARK_LENGTH        -1
#define PLAINTEXT_LENGTH        125
#define BINARY_SIZE             16
#define SALT_SIZE               0
#define BINARY_ALIGN            sizeof(ARCH_WORD_32)
#define SALT_ALIGN              sizeof(int)
#define MIN_KEYS_PER_CRYPT      1
#define MAX_KEYS_PER_CRYPT      64
#define FORMAT_TAG              "$zipmonster$"
#define TAG_LENGTH              (sizeof(FORMAT_TAG) - 1)

static struct fmt_tests zipmonster_tests[] = {
	{"$zipmonster$e0f68d6f40c5f157c169e9ca0a6f09fe", "!"},
	{"4dac447f100ee85327db2b47e295e50d", "1"},
	{NULL}
};

static char (*saved_key)[PLAINTEXT_LENGTH + 1];
static int *saved_len;
static ARCH_WORD_32 (*crypt_out)[BINARY_SIZE / sizeof(ARCH_WORD_32)];

static void init(struct fmt_main *self)
{
#ifdef _OPENMP
	static int omp_t = 1;
	omp_t = omp_get_max_threads();
	self->params.min_keys_per_crypt *= omp_t;
	omp_t *= OMP_SCALE;
	self->params.max_keys_per_crypt *= omp_t;
#endif
	saved_key = mem_calloc(self->params.max_keys_per_crypt,
			sizeof(*saved_key));
	saved_len = mem_calloc(self->params.max_keys_per_crypt,
			sizeof(*saved_len));
	crypt_out = mem_calloc(self->params.max_keys_per_crypt,
			sizeof(*crypt_out));
}

static void done(void)
{
	MEM_FREE(crypt_out);
	MEM_FREE(saved_len);
	MEM_FREE(saved_key);
}

static int valid(char *ciphertext, struct fmt_main *self)
{
	char *p = ciphertext;
	if (!strncmp(ciphertext, FORMAT_TAG, TAG_LENGTH))
		p = ciphertext + TAG_LENGTH;

	if(!p)
		return 0;
	if (!ishexlc(p))
		return 0;

	if (strlen(p) != BINARY_SIZE * 2)
		return 0;

	return 1;
}

static void *get_binary(char *ciphertext)
{
	static union {
		unsigned char c[BINARY_SIZE+1];
		ARCH_WORD dummy;
	} buf;
	unsigned char *out = buf.c;
	char *p = ciphertext;
	int i;

	if (!strncmp(ciphertext, FORMAT_TAG, TAG_LENGTH))
		p = ciphertext + TAG_LENGTH;
	for (i = 0; i < BINARY_SIZE && *p; i++) {
		out[i] =
			(atoi16[ARCH_INDEX(*p)] << 4) |
			atoi16[ARCH_INDEX(p[1])];
		p += 2;
	}

	return out;
}

static int get_hash_0(int index) { return crypt_out[index][0] & 0xf; }
static int get_hash_1(int index) { return crypt_out[index][0] & 0xff; }
static int get_hash_2(int index) { return crypt_out[index][0] & 0xfff; }
static int get_hash_3(int index) { return crypt_out[index][0] & 0xffff; }
static int get_hash_4(int index) { return crypt_out[index][0] & 0xfffff; }
static int get_hash_5(int index) { return crypt_out[index][0] & 0xffffff; }
static int get_hash_6(int index) { return crypt_out[index][0] & 0x7ffffff; }

static inline void hex_encode_uppercase(unsigned char *str, int len, unsigned char *out)
{
	int i;

	for (i = 0; i < len; ++i) {
		out[0] = itoa16u[str[i]>>4];
		out[1] = itoa16u[str[i]&0xF];
		out += 2;
	}
}

static int crypt_all(int *pcount, struct db_salt *salt)
{
	const int count = *pcount;
	int index = 0;
#ifdef _OPENMP
#pragma omp parallel for
#endif
	for (index = 0; index < count; index++)
	{
		unsigned char buffer[BINARY_SIZE];
		unsigned char hex_buffer[BINARY_SIZE * 2 + 1] = { 0 };
		MD5_CTX ctx;
		int n = 49999;

		MD5_Init(&ctx);
		MD5_Update(&ctx, saved_key[index], strlen(saved_key[index]));
		MD5_Final(buffer, &ctx);
		hex_encode_uppercase(buffer, BINARY_SIZE, hex_buffer);

		do {
			MD5_Init(&ctx);
			MD5_Update(&ctx, hex_buffer, BINARY_SIZE * 2);
			MD5_Final(buffer, &ctx);
			hex_encode_uppercase(buffer, BINARY_SIZE, hex_buffer);
			--n;
		} while (n);

		memcpy((unsigned char*)crypt_out[index], buffer, BINARY_SIZE);
	}

	return count;
}

static int cmp_all(void *binary, int count)
{
	int index = 0;
#if defined(_OPENMP) || MAX_KEYS_PER_CRYPT > 1
	for (; index < count; index++)
#endif
		if (!memcmp(binary, crypt_out[index], ARCH_SIZE))
			return 1;
	return 0;
}

static int cmp_one(void *binary, int index)
{
	return !memcmp(binary, crypt_out[index], BINARY_SIZE);
}

static int cmp_exact(char *source, int index)
{
	return 1;
}

static void zipmonster_set_key(char *key, int index)
{
	saved_len[index] =
		strnzcpyn(saved_key[index], key, sizeof(saved_key[index]));
}

static char *get_key(int index)
{
	return saved_key[index];
}

struct fmt_main fmt_zipmonster = {
	{
		FORMAT_LABEL,
		FORMAT_NAME,
		ALGORITHM_NAME,
		BENCHMARK_COMMENT,
		BENCHMARK_LENGTH,
		0,
		PLAINTEXT_LENGTH,
		BINARY_SIZE,
		BINARY_ALIGN,
		SALT_SIZE,
		SALT_ALIGN,
		MIN_KEYS_PER_CRYPT,
		MAX_KEYS_PER_CRYPT,
		FMT_CASE | FMT_8_BIT | FMT_OMP,
#if FMT_MAIN_VERSION > 11
		{ NULL },
#endif
		zipmonster_tests
	}, {
		init,
		done,
		fmt_default_reset,
		fmt_default_prepare,
		valid,
		fmt_default_split,
		get_binary,
		fmt_default_salt,
#if FMT_MAIN_VERSION > 11
		{ NULL },
#endif
		fmt_default_source,
		{
			fmt_default_binary_hash_0,
			fmt_default_binary_hash_1,
			fmt_default_binary_hash_2,
			fmt_default_binary_hash_3,
			fmt_default_binary_hash_4,
			fmt_default_binary_hash_5,
			fmt_default_binary_hash_6
		},
		fmt_default_salt_hash,
		NULL,
		fmt_default_set_salt,
		zipmonster_set_key,
		get_key,
		fmt_default_clear_keys,
		crypt_all,
		{
			get_hash_0,
			get_hash_1,
			get_hash_2,
			get_hash_3,
			get_hash_4,
			get_hash_5,
			get_hash_6
		},
		cmp_all,
		cmp_one,
		cmp_exact
	}
};

#endif /* plugin stanza */