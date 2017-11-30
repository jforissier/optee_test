/*
 * Copyright (c) 2017, Linaro Limited
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <ctype.h>
#include <err.h>
#include <inttypes.h>
#include <pta_secstor_ta_mgmt.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <tee_client_api.h>
#include <unistd.h>
#include "xtest_test.h"
#include "uninstall_ta.h"

static void uuid_to_octets(uint8_t d[16], const TEEC_UUID *s)
{
        d[0] = s->timeLow >> 24;
        d[1] = s->timeLow >> 16;
        d[2] = s->timeLow >> 8;
        d[3] = s->timeLow;
        d[4] = s->timeMid >> 8;
        d[5] = s->timeMid;
        d[6] = s->timeHiAndVersion >> 8;
        d[7] = s->timeHiAndVersion;
        memcpy(d + 8, s->clockSeqAndNode, sizeof(s->clockSeqAndNode));
}

static bool parse_uuid(uint8_t bin_uuid[16], const char *uuid)
{
	TEEC_UUID teec_uuid;
	const char *p;
	char byte[3];
	int i;

	if (strlen(uuid) != 36)
		return false;

	for (i = 0; i < 36; i++) {
		if (!isxdigit(uuid[i])) {
			if (i == 8 || i == 13 || i == 18 || i == 23) {
				if (uuid[i] != '-')
					return false;
			} else {
				return false;
			}
		}
	}

	teec_uuid.timeLow = strtoul(uuid, NULL, 16);
	teec_uuid.timeMid = strtoul(uuid + 9, NULL, 16);
	teec_uuid.timeHiAndVersion = strtoul(uuid + 14, NULL, 16);

	p = uuid + 19;
	byte[3] = '\0';
	for (i = 0; i < 8; i++) {
		if  (*p == '-')
			p++;
		byte[0] = *p++;
		byte[1] = *p++;
		teec_uuid.clockSeqAndNode[i] = strtoul(byte, NULL, 16);
	}

	uuid_to_octets(bin_uuid, &teec_uuid);

	return true;
}

static void uninstall_ta(TEEC_Session *sess, char *uuid)
{
	TEEC_Result res;
	uint32_t err_origin;
	TEEC_Operation op;
	uint8_t bin_uuid[16];

	if (!parse_uuid(bin_uuid, uuid))
		errx(1, "Invalid UUID\n");

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT, TEEC_NONE,
					 TEEC_NONE, TEEC_NONE);
	op.params[0].tmpref.buffer = bin_uuid;
	op.params[0].tmpref.size = sizeof(bin_uuid);

	res = TEEC_InvokeCommand(sess, PTA_SECSTOR_TA_MGMT_UNINSTALL, &op,
				 &err_origin);
	if (res)
		errx(1, "uninstall_ta: TEEC_InvokeCommand: %#" PRIx32
			" err_origin %#" PRIx32, res, err_origin);
}

int uninstall_ta_runner_cmd_parser(int argc, char *argv[])
{
	TEEC_Result res;
	uint32_t err_origin;
	TEEC_UUID uuid = PTA_SECSTOR_TA_MGMT_UUID;
	TEEC_Context ctx;
	TEEC_Session sess;

	if (argc != 2)
		errx(1, "Argument expected (UUID)\n");

	res = TEEC_InitializeContext(NULL, &ctx);
	if (res)
		errx(1, "TEEC_InitializeContext: %#" PRIx32, res);

	res = TEEC_OpenSession(&ctx, &sess, &uuid, TEEC_LOGIN_PUBLIC, NULL,
			       NULL, &err_origin);
	if (res)
		errx(1, "TEEC_OpenSession: res %#" PRIx32 " err_orig %#" PRIx32,
			res, err_origin);

	uninstall_ta(&sess, argv[1]);

	TEEC_CloseSession(&sess);
	TEEC_FinalizeContext(&ctx);

	return 0;
}
