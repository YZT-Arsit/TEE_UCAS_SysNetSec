## 基于 OP-TEE 的安全 HMAC 服务设计与实现

## 一、任务概述

可信执行环境（TEE, Trusted Execution Environment）能够在普通操作系统（Normal World）之外提供一个隔离执行环境（Secure World），用于保护密钥、敏感计算逻辑和可信服务接口。本实验要求基于 **OP-TEE** 环境，设计并实现一个 **安全 HMAC 服务**：

* 在 **TA（Trusted Application）** 中安全生成/保存 HMAC 密钥，并在 TA 内完成 HMAC-SHA256 运算；
* 在 **CA（Client Application）** 中调用 TA 提供的命令接口，实现密钥管理、消息认证码生成、认证码校验等功能；
* 在 OP-TEE QEMU（Buildroot）环境中完成部署、运行与测试，记录实验现象与问题分析。

---

## 二、实验目标

1. 理解 **Normal World / Secure World** 的基本职责划分；
2. 理解 OP-TEE 中 **CA ↔ tee-supplicant ↔ TEE Core ↔ TA** 的调用路径；
3. 掌握 TA 的基本结构与生命周期（Create/OpenSession/InvokeCommand/CloseSession）；
4. 掌握 CA 通过 `libteec` 调用 TA 的方法（Context / Session / Operation）；
5. 实现一个具备实际安全意义的 TEE 服务（密钥在 TA 内生成与使用）；
6. 在 QEMU + Buildroot 环境中完成部署、测试和问题定位。

---

## 三、实验环境

* Ubuntu 22.04/24.04
* OP-TEE QEMU 环境（配置说明见 `env/README.md`）
* GCC / Make / Git
* OP-TEE 相关组件：

  * `optee_os`
  * `optee_client`
  * `trusted-firmware-a`
  * `u-boot`
  * `linux`
  * `buildroot`
  * `qemu`

## 四、任务要求

### 子实验一：环境全栈启动与双世界日志追踪（10分）

任务：成功编译全栈镜像并启动 QEMU，需同时展示 Normal World 和 Secure World 两个控制台 。

要求：在 Normal World 运行 xtest 进行基准测试，并截取 Secure World 窗口输出的初始化日志。


### 子实验二：TA/CA 工程搭建与基础连通性（20分）

实现一个最小可用的 OP-TEE 应用工程，完成 TA 与 CA 的基础调用闭环。

**要求：**

* 正确划分工程目录（`host/`、`ta/`、`include/` 等）；
* 定义 TA UUID，并在 CA 与 TA 中保持一致；
* CA 能成功：

  * 初始化 `TEEC_Context`
  * 打开 `TEEC_Session`
  * 调用至少一个 TA 命令（如 `CMD_PING` / `CMD_INFO`）
* TA 返回成功状态码，CA 正确打印结果。

**验收点：**

* 能展示 TA 被成功加载（放置于 `/lib/optee_armtz/`）；
* 能展示 CA 在 Buildroot 中执行成功；
* 能说明一次调用链条的关键步骤。

---

### 子实验三：安全密钥生成与状态查询（25分）

在 TA 内生成并保存 HMAC 密钥（建议 32 字节，用于 HMAC-SHA256），并提供状态查询接口。

**要求：**

* TA 提供命令接口，例如：

  * `CMD_GEN_KEY`：生成新密钥（覆盖或首次生成）
  * `CMD_INFO`：返回密钥状态（是否存在、长度等）
* 密钥必须在 TA 内部管理（禁止在 CA 端保存明文密钥）
* CA 提供 CLI 命令调用（如 `genkey`、`info`）

**验收点：**

* `info` 在 `genkey` 前后状态变化正确；
* `genkey` 成功后可用于后续 HMAC 运算。

---

### 子实验四：HMAC-SHA256 计算接口（25分）

实现 TA 内部的 HMAC 运算能力，CA 提供消息输入并输出十六进制 HMAC 值。

**要求：**

* TA 提供 `CMD_HMAC` 命令：

  * 输入：消息（字符串或字节数组）
  * 输出：HMAC-SHA256（32字节）
* CA 命令示例：

  * `./secure_hmac_ca hmac "hello-optee"`
* 输出格式规范：

  * 以十六进制打印完整 32 字节结果

**验收点：**

* 对同一消息和同一密钥，输出稳定一致；
* 重新生成密钥后，同一消息输出变化；
* 正确处理缓冲区长度与参数类型。

---

### 子实验五：HMAC 校验接口（Verify）（20分）

在 TA 内部实现 HMAC 校验逻辑，CA 提供“消息 + 十六进制 MAC”输入进行验证。

**要求：**

* TA 提供 `CMD_VERIFY` 命令：

  * 输入：消息、待校验 MAC
  * 输出：成功/失败状态
* CA 命令示例：

  * `./secure_hmac_ca verify "hello-optee" <hex_mac>`
* 要求给出明确返回信息：

  * `verify: OK`
  * `verify: FAIL (MAC mismatch)`

**验收点：**

* 正确 MAC 校验通过；
* 篡改消息或 MAC 后校验失败；
* 错误输入（长度不对、非 hex 字符）有基本处理。
---

## 五、建议命令行功能

实现的 CA 至少支持以下命令（可自行扩展）：

```bash
./secure_hmac_ca info
./secure_hmac_ca genkey
./secure_hmac_ca hmac "hello-optee"
./secure_hmac_ca verify "hello-optee" <hex_mac>
```

### 说明

* `info`：输出密钥是否存在、长度
* `genkey`：生成或覆盖密钥
* `hmac`：输出十六进制 HMAC
* `verify`：输出验证结果（OK/FAIL）

---

## 六、工程结构建议

```text
tee_lab/
├── include/
│   └── secure_hmac_ta.h          # CA/TA共享头文件（UUID、CMD定义）
├── host/
│   ├── main.c                    # CA实现（CLI + libteec 调用）
│   └── Makefile
├── ta/
│   ├── secure_hmac_ta.c          # TA命令逻辑
│   ├── user_ta_header_defines.h  # TA头定义（UUID、堆栈、标志）
│   ├── sub.mk
│   └── Makefile
└── README.md
```

---

## 七、实验步骤

### A. 宿主机侧编译

1. 设置 `TA_DEV_KIT_DIR`（指向 `optee_os/out/.../export-ta_arm64`）
2. 编译 TA（在 `tee_lab/ta/`）
3. 编译 CA（在 `tee_lab/host/`）
4. 确认生成：

   * `xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx.ta`
   * `secure_hmac_ca`

### B. 启动 OP-TEE QEMU（Buildroot）

1. 启动 `optee-qemu`（进入 Secure/Normal World 窗口）
2. 在 QEMU Monitor 输入 `c` 继续执行（如果启动停在 `(qemu)`）
3. 等待 Buildroot 启动完成，登录 `root`

### C. 部署与运行

1. 将 TA 拷贝到 `/lib/optee_armtz/`
2. 将 CA 拷贝到 `/root/`
3. `chmod +x /root/secure_hmac_ca`
4. 运行测试：

   * `info`
   * `genkey`
   * `hmac`
   * `verify`

### D. 结果验证

* 正确性测试：同一输入同一密钥结果一致
* 失败测试：修改消息/MAC 后验证失败
* 记录日志与截图

---

## 八、评分细则（本任务内 100 分制）

### 1）功能完成度（60分）

* TA/CA 基础连通与命令调用（20分）
* 密钥生成与状态查询（15分）
* HMAC 计算（15分）
* HMAC 校验（10分）

### 2）工程质量（20分）

* 工程结构清晰、文件组织合理（8分）
* 代码风格统一、注释清晰（6分）
* 错误处理与返回值判断较完整（6分）

### 3）实验过程与分析（20分）

* 环境/版本/步骤记录完整（8分）
* 关键现象截图与解释（6分）
* 问题定位与解决方案分析（6分）

---

## 九、进阶要求

### 进阶一：密钥持久化（+6）

* 使用 OP-TEE Persistent Object API 将 HMAC 密钥持久化到安全存储；
* 重启后仍能 `info` 显示存在，并继续 `hmac/verify`。

### 进阶二：多会话隔离与权限控制（+4）

* 支持多 session 调用；
* 区分管理命令（`genkey`）与普通命令（`hmac/verify`）的访问策略（可简化为 session 标志或口令）。

### 进阶三：大消息分块处理（+4）

* 支持超过单次缓冲区上限的消息；
* 在 TA 内实现分块 HMAC 更新流程（或在 CA 层切分并设计接口）。

### 进阶四：自动化测试脚本（+3）

* 提供脚本自动执行：

  * `genkey`
  * 多组 `hmac`
  * `verify` 正反例测试
* 自动输出测试结果汇总。

### 进阶五：安全性分析与对比实验（+3）

* 对比密钥在 CA 中计算 HMAC与密钥在 TA 中计算 HMAC的攻击面差异；
* 分析 TEE 方案的边界。

---
