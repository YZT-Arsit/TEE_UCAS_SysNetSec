#ifndef SECURE_HMAC_TA_H
#define SECURE_HMAC_TA_H
#ifndef TEE_ERROR_MAC_INVALID
#define TEE_ERROR_MAC_INVALID 0xFFFF3071
#endif
/*
 * UUID: 8f6c0d2e-9c5a-4b53-a2ad-6f3b6f8f1123
 * 可换
 */
#define TA_SECURE_HMAC_UUID \
	{ 0x8f6c0d2e, 0x9c5a, 0x4b53, \
	  { 0xa2, 0xad, 0x6f, 0x3b, 0x6f, 0x8f, 0x11, 0x23 } }

/* TA 命令号 */
#define CMD_GEN_KEY      0
#define CMD_HMAC         1
#define CMD_VERIFY       2
#define CMD_DEL_KEY      3
#define CMD_INFO         4

/* 固定 HMAC 输出长度（SHA-256） */
#define HMAC_SHA256_LEN 32

#endif
