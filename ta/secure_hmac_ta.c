#include <tee_internal_api.h>
#include <tee_internal_api_extensions.h>
#include "secure_hmac_ta.h"

#define KEY_SIZE_BYTES 32
#define KEY_OBJ_ID     "hmac_key_v1"
#define KEY_OBJ_ID_LEN 11

/* ---------------------------
 * 加载/保存/删除密钥
 * --------------------------- */

static TEE_Result load_key_from_storage(uint8_t *key, uint32_t *key_len)
{
	TEE_ObjectHandle obj = TEE_HANDLE_NULL;
	TEE_Result res = TEE_SUCCESS;
	uint32_t read_bytes = 0;

	if (!key || !key_len || *key_len < KEY_SIZE_BYTES)
		return TEE_ERROR_BAD_PARAMETERS;

	res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
				       KEY_OBJ_ID, KEY_OBJ_ID_LEN,
				       TEE_DATA_FLAG_ACCESS_READ,
				       &obj);
	if (res != TEE_SUCCESS)
		return res;

	res = TEE_ReadObjectData(obj, key, *key_len, &read_bytes);
	TEE_CloseObject(obj);

	if (res != TEE_SUCCESS)
		return res;

	if (read_bytes != KEY_SIZE_BYTES)
		return TEE_ERROR_CORRUPT_OBJECT;

	*key_len = read_bytes;
	return TEE_SUCCESS;
}

static TEE_Result save_key_to_storage(const uint8_t *key, uint32_t key_len)
{
	TEE_ObjectHandle obj = TEE_HANDLE_NULL;
	TEE_Result res = TEE_SUCCESS;

	if (!key || key_len != KEY_SIZE_BYTES)
		return TEE_ERROR_BAD_PARAMETERS;

	(void)TEE_CloseAndDeletePersistentObject1(obj);

	res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
				       KEY_OBJ_ID, KEY_OBJ_ID_LEN,
				       TEE_DATA_FLAG_ACCESS_READ |
				       TEE_DATA_FLAG_ACCESS_WRITE_META,
				       &obj);
	if (res == TEE_SUCCESS) {
		TEE_CloseAndDeletePersistentObject1(obj);
		obj = TEE_HANDLE_NULL;
	}

	res = TEE_CreatePersistentObject(TEE_STORAGE_PRIVATE,
					 KEY_OBJ_ID, KEY_OBJ_ID_LEN,
					 TEE_DATA_FLAG_ACCESS_READ |
					 TEE_DATA_FLAG_ACCESS_WRITE |
					 TEE_DATA_FLAG_ACCESS_WRITE_META |
					 TEE_DATA_FLAG_OVERWRITE,
					 TEE_HANDLE_NULL,
					 key, key_len,
					 &obj);
	if (res != TEE_SUCCESS)
		return res;

	TEE_CloseObject(obj);
	return TEE_SUCCESS;
}

static TEE_Result delete_key_from_storage(void)
{
	TEE_ObjectHandle obj = TEE_HANDLE_NULL;
	TEE_Result res = TEE_SUCCESS;

	res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
				       KEY_OBJ_ID, KEY_OBJ_ID_LEN,
				       TEE_DATA_FLAG_ACCESS_READ |
				       TEE_DATA_FLAG_ACCESS_WRITE_META,
				       &obj);
	if (res != TEE_SUCCESS)
		return res;

	TEE_CloseAndDeletePersistentObject1(obj);
	return TEE_SUCCESS;
}

static bool key_exists(void)
{
	TEE_ObjectHandle obj = TEE_HANDLE_NULL;
	TEE_Result res = TEE_OpenPersistentObject(TEE_STORAGE_PRIVATE,
						  KEY_OBJ_ID, KEY_OBJ_ID_LEN,
						  TEE_DATA_FLAG_ACCESS_READ,
						  &obj);
	if (res == TEE_SUCCESS) {
		TEE_CloseObject(obj);
		return true;
	}
	return false;
}

/* ---------------------------
 * HMAC 核心
 * --------------------------- */
static TEE_Result compute_hmac_sha256(const uint8_t *msg, uint32_t msg_len,
				      uint8_t *out_mac, uint32_t *out_len)
{
	TEE_Result res;
	TEE_OperationHandle op = TEE_HANDLE_NULL;
	TEE_ObjectHandle key_obj = TEE_HANDLE_NULL;
	TEE_Attribute attr;
	uint8_t key[KEY_SIZE_BYTES];
	uint32_t key_len = sizeof(key);

	if (!msg || !out_mac || !out_len)
		return TEE_ERROR_BAD_PARAMETERS;

	if (*out_len < HMAC_SHA256_LEN)
		return TEE_ERROR_SHORT_BUFFER;

	res = load_key_from_storage(key, &key_len);
	if (res != TEE_SUCCESS)
		return res;

	/* HMAC key */
	res = TEE_AllocateTransientObject(TEE_TYPE_HMAC_SHA256, key_len * 8, &key_obj);
	if (res != TEE_SUCCESS)
		goto out;

	TEE_InitRefAttribute(&attr, TEE_ATTR_SECRET_VALUE, key, key_len);
	res = TEE_PopulateTransientObject(key_obj, &attr, 1);
	if (res != TEE_SUCCESS)
		goto out;

	res = TEE_AllocateOperation(&op, TEE_ALG_HMAC_SHA256, TEE_MODE_MAC, key_len * 8);
	if (res != TEE_SUCCESS)
		goto out;

	res = TEE_SetOperationKey(op, key_obj);
	if (res != TEE_SUCCESS)
		goto out;

	TEE_MACInit(op, NULL, 0);
	TEE_MACUpdate(op, msg, msg_len);

	/* Final 输出 HMAC */
	res = TEE_MACComputeFinal(op, NULL, 0, out_mac, out_len);

out:
	TEE_MemFill(key, 0, sizeof(key));
	if (op != TEE_HANDLE_NULL)
		TEE_FreeOperation(op);
	if (key_obj != TEE_HANDLE_NULL)
		TEE_FreeTransientObject(key_obj);
	return res;
}

/* ---------------------------
 * 命令处理
 * --------------------------- */

static TEE_Result cmd_gen_key(uint32_t param_types, TEE_Param params[4])
{
	const uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE);
	uint8_t key[KEY_SIZE_BYTES];
	TEE_Result res;

	(void)params;

	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	TEE_GenerateRandom(key, sizeof(key));
	res = save_key_to_storage(key, sizeof(key));
	TEE_MemFill(key, 0, sizeof(key));
	return res;
}

static TEE_Result cmd_hmac(uint32_t param_types, TEE_Param params[4])
{
	const uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,
					     TEE_PARAM_TYPE_MEMREF_OUTPUT,
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE);
	TEE_Result res;
	uint32_t mac_len;

	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	mac_len = params[1].memref.size;
	res = compute_hmac_sha256(params[0].memref.buffer,
				  params[0].memref.size,
				  params[1].memref.buffer,
				  &mac_len);
	params[1].memref.size = mac_len;
	return res;
}

static TEE_Result cmd_verify(uint32_t param_types, TEE_Param params[4])
{
	const uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_MEMREF_INPUT,  /* msg */
					     TEE_PARAM_TYPE_MEMREF_INPUT,  /* mac */
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE);
	TEE_Result res;
	uint8_t calc[HMAC_SHA256_LEN];
	uint32_t calc_len = sizeof(calc);

	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	if (params[1].memref.size != HMAC_SHA256_LEN)
		return TEE_ERROR_BAD_PARAMETERS;

	res = compute_hmac_sha256(params[0].memref.buffer,
				  params[0].memref.size,
				  calc, &calc_len);
	if (res != TEE_SUCCESS)
		return res;

	if (calc_len != params[1].memref.size) {
		TEE_MemFill(calc, 0, sizeof(calc));
		return TEE_ERROR_SECURITY;
	}

	/* 常量时间比较 */
	if (TEE_MemCompare(calc, params[1].memref.buffer, calc_len) != 0) {
		TEE_MemFill(calc, 0, sizeof(calc));
		return TEE_ERROR_MAC_INVALID;
	}

	TEE_MemFill(calc, 0, sizeof(calc));
	return TEE_SUCCESS;
}

static TEE_Result cmd_del_key(uint32_t param_types, TEE_Param params[4])
{
	const uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE);
	(void)params;

	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	return delete_key_from_storage();
}

static TEE_Result cmd_info(uint32_t param_types, TEE_Param params[4])
{
	const uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_VALUE_OUTPUT,
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE);

	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	params[0].value.a = key_exists() ? 1 : 0;
	params[0].value.b = KEY_SIZE_BYTES;
	return TEE_SUCCESS;
}

/* ---------------------------
 * TA 标准入口
 * --------------------------- */

TEE_Result TA_CreateEntryPoint(void)
{
	IMSG("secure_hmac_ta: CreateEntry");
	return TEE_SUCCESS;
}

void TA_DestroyEntryPoint(void)
{
	IMSG("secure_hmac_ta: DestroyEntry");
}

TEE_Result TA_OpenSessionEntryPoint(uint32_t param_types,
				    TEE_Param params[4],
				    void **sess_ctx)
{
	const uint32_t exp = TEE_PARAM_TYPES(TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE,
					     TEE_PARAM_TYPE_NONE);
	(void)params;
	(void)sess_ctx;

	if (param_types != exp)
		return TEE_ERROR_BAD_PARAMETERS;

	IMSG("secure_hmac_ta: OpenSession");
	return TEE_SUCCESS;
}

void TA_CloseSessionEntryPoint(void *sess_ctx)
{
	(void)sess_ctx;
	IMSG("secure_hmac_ta: CloseSession");
}

TEE_Result TA_InvokeCommandEntryPoint(void *sess_ctx,
				      uint32_t cmd_id,
				      uint32_t param_types,
				      TEE_Param params[4])
{
	(void)sess_ctx;

	switch (cmd_id) {
	case CMD_GEN_KEY:
		return cmd_gen_key(param_types, params);
	case CMD_HMAC:
		return cmd_hmac(param_types, params);
	case CMD_VERIFY:
		return cmd_verify(param_types, params);
	case CMD_DEL_KEY:
		return cmd_del_key(param_types, params);
	case CMD_INFO:
		return cmd_info(param_types, params);
	default:
		return TEE_ERROR_NOT_SUPPORTED;
	}
}
