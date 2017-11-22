/*
 * Copyright (c) 2017, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <dirent.h>
#include <stdint.h>
#include <stdbool.h>
#include <fnmatch.h>
#include <compiler.h>
#include <inttypes.h>
#include <err.h>
#include <pta_management.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <tee_client_api.h>
#include "xtest_test.h"
#include "install_tas.h"

static void *read_ta(const char *dname, const char *fname, size_t *size)
{
	char nbuf[PATH_MAX];
	FILE *f;
	void *buf;
	size_t s;

	snprintf(nbuf, sizeof(nbuf), "%s/%s", dname, fname);

	f = fopen(nbuf, "rb");
	if (!f)
		err(1, "fopen(\"%s\")", nbuf);

	if (fseek(f, 0, SEEK_END))
		err(1, "fseek");

	s = ftell(f);
	rewind(f);

	buf = malloc(s);
	if (!buf)
		err(1, "malloc(%zu)", s);

	if (fread(buf, 1, s, f) != s)
		err(1, "fread");

	*size = s;
	return buf;
}

static void install_ta(TEEC_Session *sess, void *buf, size_t blen)
{
	TEEC_Result res;
	uint32_t err_origin;
	TEEC_Operation op;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					 TEEC_NONE, TEEC_NONE);
	op.params[0].tmpref.buffer = buf;
	op.params[0].tmpref.size = blen;

	res = TEEC_InvokeCommand(sess, PTA_MANAGEMENT_BOOTSTRAP, &op,
				 &err_origin);
	if (res == PTA_MANAGEMENT_TA_EXISTS)
		printf("TA is already installed\n");
	else if (res)
		errx(1, "install_ta: TEEC_InvokeCommand: %#" PRIx32 " err_origin %#" PRIx32, res, err_origin);
}

int install_tas_runner_cmd_parser(int argc __unused, char *argv[] __unused)
{
	TEEC_Result res;
	uint32_t err_origin;
	TEEC_UUID uuid = PTA_MANAGEMENT_UUID;
	TEEC_Context ctx;
	TEEC_Session sess;
	const char *ta_dir = TA_DIR;
	DIR *dirp;

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res)
		errx(1, "TEEC_InitializeContext: %#" PRIx32, res);

	res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL,
			       NULL, &err_origin);
	if (res)
		errx(1, "TEEC_OpenSession: res %#" PRIx32 " err_orig %#" PRIx32,
			res, err_origin);

	printf("Searching for bootstrap TAs in \"%s\"\n", ta_dir);
	dirp = opendir(ta_dir);
	if (!dirp)
		err(1, "opendir(\"%s\")", ta_dir);
	while (true) {
		struct dirent *dent = readdir(dirp);
		void *ta;
		size_t ta_size;

		if (!dent)
			break;

		if (fnmatch("*.bsta", dent->d_name, 0))
			continue;

		printf("Installing \"%s\"\n", dent->d_name);
		ta = read_ta(ta_dir, dent->d_name, &ta_size);
		install_ta(&sess, ta, ta_size);
	}

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	printf("Installing TAs done\n");

	return 0;
}
