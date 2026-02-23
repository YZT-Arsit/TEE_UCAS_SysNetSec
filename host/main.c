#include <err.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include <tee_client_api.h>
#include "secure_hmac_ta.h"

#define MAX_MSG_LEN   4096

static const TEEC_UUID ta_uuid = TA_SECURE_HMAC_UUID;

static void hex_print(const uint8_t *buf, size_t len)
{
	for (size_t i = 0; i < len; i++)
		printf("%02x", buf[i]);
	printf("\n");
}

static int hex_val(char c)
{
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
	if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
	return -1;
}

static int hex_to_bytes(const char *hex, uint8_t *out, size_t out_sz, size_t *out_len)
{
	size_t n = strlen(hex);
	if (n % 2 != 0)
		return -1;
	if (out_sz < n / 2)
		return -1;

	for (size_t i = 0; i < n; i += 2) {
		int hi = hex_val(hex[i]);
		int lo = hex_val(hex[i + 1]);
		if (hi < 0 || lo < 0)
			return -1;
		out[i / 2] = (uint8_t)((hi << 4) | lo);
	}
	*out_len = n / 2;
	return 0;
}

static void usage(const char *prog)
{
	printf("Usage:\n");
	printf("  %s genkey\n", prog);
	printf("  %s info\n", prog);
	printf("  %s hmac <message>\n", prog);
	printf("  %s verify <message> <hexmac>\n", prog);
	printf("  %s delkey\n", prog);
	printf("  %s bench <count> <message>\n", prog);
}

/* 初始化上下文和会话 */
static void open_ta(TEEC_Context *ctx, TEEC_Session *sess)
{
	TEEC_Result res;
	uint32_t err_origin;

	res = TEEC_InitializeContext(NULL, ctx);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_InitializeContext failed: 0x%x", res);

	res = TEEC_OpenSession(ctx, sess, &ta_uuid, TEEC_LOGIN_PUBLIC,
			       NULL, NULL, &err_origin);
	if (res != TEEC_SUCCESS)
		errx(1, "TEEC_OpenSession failed: 0x%x origin=0x%x", res, err_origin);
}

static void close_ta(TEEC_Context *ctx, TEEC_Session *sess)
{
	TEEC_CloseSession(sess);
	TEEC_FinalizeContext(ctx);
}

static int cmd_genkey(TEEC_Session *sess)
{
	TEEC_Operation op = { 0 };
	uint32_t err_origin = 0;
	TEEC_Result res;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(sess, CMD_GEN_KEY, &op, &err_origin);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr, "genkey failed: 0x%x origin=0x%x\n", res, err_origin);
		return 1;
	}

	printf("genkey: success\n");
	return 0;
}

static int cmd_info(TEEC_Session *sess)
{
	TEEC_Operation op = { 0 };
	uint32_t err_origin = 0;
	TEEC_Result res;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_OUTPUT, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(sess, CMD_INFO, &op, &err_origin);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr, "info failed: 0x%x origin=0x%x\n", res, err_origin);
		return 1;
	}

	printf("key_exists=%u, key_size=%u bytes\n",
	       op.params[0].value.a, op.params[0].value.b);
	return 0;
}

static int cmd_hmac(TEEC_Session *sess, const char *msg)
{
	TEEC_Operation op = { 0 };
	uint32_t err_origin = 0;
	TEEC_Result res;
	uint8_t mac[HMAC_SHA256_LEN];
	size_t msg_len = strlen(msg);

	if (msg_len > MAX_MSG_LEN) {
		fprintf(stderr, "message too long\n");
		return 1;
	}

	memset(mac, 0, sizeof(mac));

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_NONE, TEEC_NONE);

	op.params[0].tmpref.buffer = (void *)msg;
	op.params[0].tmpref.size = msg_len;
	op.params[1].tmpref.buffer = mac;
	op.params[1].tmpref.size = sizeof(mac);

	res = TEEC_InvokeCommand(sess, CMD_HMAC, &op, &err_origin);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr, "hmac failed: 0x%x origin=0x%x\n", res, err_origin);
		if (res == TEEC_ERROR_ITEM_NOT_FOUND)
			fprintf(stderr, "hint: key not found, run genkey first\n");
		return 1;
	}

	printf("HMAC-SHA256(%s) = ", msg);
	hex_print(mac, op.params[1].tmpref.size);
	return 0;
}

static int cmd_verify(TEEC_Session *sess, const char *msg, const char *hexmac)
{
	TEEC_Operation op = { 0 };
	uint32_t err_origin = 0;
	TEEC_Result res;
	uint8_t mac[HMAC_SHA256_LEN];
	size_t mac_len = 0;

	if (hex_to_bytes(hexmac, mac, sizeof(mac), &mac_len) != 0) {
		fprintf(stderr, "invalid hexmac\n");
		return 1;
	}
	if (mac_len != HMAC_SHA256_LEN) {
		fprintf(stderr, "mac length must be %d bytes (64 hex chars)\n", HMAC_SHA256_LEN);
		return 1;
	}

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_INPUT,
					 TEEC_NONE, TEEC_NONE);

	op.params[0].tmpref.buffer = (void *)msg;
	op.params[0].tmpref.size = strlen(msg);
	op.params[1].tmpref.buffer = mac;
	op.params[1].tmpref.size = mac_len;

	res = TEEC_InvokeCommand(sess, CMD_VERIFY, &op, &err_origin);
	if (res == TEEC_SUCCESS) {
		printf("verify: OK\n");
		return 0;
	}

	if (res == TEE_ERROR_MAC_INVALID) {
		printf("verify: FAIL (MAC mismatch)\n");
		return 2;
	}

	fprintf(stderr, "verify failed: 0x%x origin=0x%x\n", res, err_origin);
	return 1;
}

static int cmd_delkey(TEEC_Session *sess)
{
	TEEC_Operation op = { 0 };
	uint32_t err_origin = 0;
	TEEC_Result res;

	op.paramTypes = TEEC_PARAM_TYPES(TEEC_NONE, TEEC_NONE, TEEC_NONE, TEEC_NONE);

	res = TEEC_InvokeCommand(sess, CMD_DEL_KEY, &op, &err_origin);
	if (res != TEEC_SUCCESS) {
		fprintf(stderr, "delkey failed: 0x%x origin=0x%x\n", res, err_origin);
		return 1;
	}

	printf("delkey: success\n");
	return 0;
}

static uint64_t now_ns(void)
{
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (uint64_t)ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

static int cmd_bench(TEEC_Session *sess, int count, const char *msg)
{
	if (count <= 0) {
		fprintf(stderr, "count must be > 0\n");
		return 1;
	}

	uint8_t mac[HMAC_SHA256_LEN];
	uint32_t err_origin = 0;
	TEEC_Result res;
	uint64_t start, end;
	double avg_us;

	TEEC_Operation op = { 0 };
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_MEMREF_TEMP_INPUT,
					 TEEC_MEMREF_TEMP_OUTPUT,
					 TEEC_NONE, TEEC_NONE);
	op.params[0].tmpref.buffer = (void *)msg;
	op.params[0].tmpref.size = strlen(msg);
	op.params[1].tmpref.buffer = mac;

	start = now_ns();
	for (int i = 0; i < count; i++) {
		op.params[1].tmpref.size = sizeof(mac);
		res = TEEC_InvokeCommand(sess, CMD_HMAC, &op, &err_origin);
		if (res != TEEC_SUCCESS) {
			fprintf(stderr, "bench invoke failed at %d: 0x%x origin=0x%x\n",
			        i, res, err_origin);
			return 1;
		}
	}
	end = now_ns();

	avg_us = (double)(end - start) / count / 1000.0;
	printf("bench: count=%d, avg=%.2f us/call, last_mac=", count, avg_us);
	hex_print(mac, sizeof(mac));
	return 0;
}

int main(int argc, char *argv[])
{
	TEEC_Context ctx;
	TEEC_Session sess;
	int ret = 0;

	if (argc < 2) {
		usage(argv[0]);
		return 1;
	}

	open_ta(&ctx, &sess);

	if (strcmp(argv[1], "genkey") == 0) {
		ret = cmd_genkey(&sess);
	} else if (strcmp(argv[1], "info") == 0) {
		ret = cmd_info(&sess);
	} else if (strcmp(argv[1], "hmac") == 0) {
		if (argc != 3) {
			usage(argv[0]);
			ret = 1;
		} else {
			ret = cmd_hmac(&sess, argv[2]);
		}
	} else if (strcmp(argv[1], "verify") == 0) {
		if (argc != 4) {
			usage(argv[0]);
			ret = 1;
		} else {
			ret = cmd_verify(&sess, argv[2], argv[3]);
		}
	} else if (strcmp(argv[1], "delkey") == 0) {
		ret = cmd_delkey(&sess);
	} else if (strcmp(argv[1], "bench") == 0) {
		if (argc != 4) {
			usage(argv[0]);
			ret = 1;
		} else {
			ret = cmd_bench(&sess, atoi(argv[2]), argv[3]);
		}
	} else {
		usage(argv[0]);
		ret = 1;
	}

	close_ta(&ctx, &sess);
	return ret;
}
