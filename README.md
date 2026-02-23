# OP-TEE 实验环境搭建指南 (QEMU ARMv8)

本指南旨在帮助学生在个人机器上搭建基于 **QEMU** 的 **OP-TEE (Open Portable Trusted Execution Environment)** 仿真开发环境。

---

## 0. 硬件与系统要求

* **宿主机**: Windows 或 macOS (建议 Apple Silicon M1/M2/M3)。
* **虚拟机**: **Ubuntu 24.04 LTS (ARM64)**。
* **磁盘空间**: 虚拟机硬盘容量需分配 **至少 50GB**。
* **内存**: 建议分配 **至少 8GB** 内存以防止编译时内存溢出 (OOM)。

---

## 1. 基础依赖安装

在 Ubuntu 24.04 终端执行以下命令，安装编译所需的依赖包。

> **注意**：针对 24.04 版本，部分包名已进行适配调整。

```bash
sudo apt-get update
sudo apt-get install -y adb fastboot autoconf automake bc bison build-essential \
    ccache cscope cppcheck curl device-tree-compiler expect flex ftp gdisk \
    libattr1-dev libcap-dev libfdt-dev libftdi-dev libglib2.0-dev libhidapi-dev \
    libncurses-dev libpixman-1-dev libssl-dev libtool make mtools \
    netcat-openbsd python3-cryptography python3-pip python3-pyelftools \
    python3-serial python3-yaml rsync unzip uuid-dev xdg-utils xterm zlib1g-dev

```

---

## 2. 磁盘空间检查与 LVM 扩容

1. **检查剩余空间**：执行 `df -h`。如果可用空间小于 5G，必须扩容。
2. **执行扩容命令**：
```bash
sudo lvextend -l +100%FREE /dev/mapper/ubuntu--vg-ubuntu--lv
sudo resize2fs /dev/mapper/ubuntu--vg-ubuntu--lv

```


执行后，可用空间应显著增加。

---

## 3. 源码同步

### 3.1 安装 Repo 工具

```bash
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
export PATH=$PATH:~/bin

```

### 3.2 配置 Git 身份

不配置身份会导致 `repo init` 失败。

```bash
git config --global user.email "you@example.com"
git config --global user.name "Your Name"

```

### 3.3 初始化与同步

```bash
mkdir -p ~/optee-qemu && cd ~/optee-qemu
repo init -u https://github.com/OP-TEE/manifest.git -m qemu_v8.xml
repo sync -j$(nproc)

```

---

## 4. 构建过程

### 4.1 安装工具链

```bash
cd ~/optee-qemu/build
make -j$(nproc) toolchains

```

> **提示**：如果 Rust 安装过程中终端长时间无显，可尝试按下回车或 `Ctrl + C` 后再次运行以补齐组件。

### 4.2 全量编译

为避免 `ccache` 参数冲突导致的 `invalid option -- 'f'` 报错，建议显式禁用缓存进行全量编译。

```bash
make -j$(nproc) CCACHE=

```

---

## 5. 运行与验证

### 5.1 启动仿真器

```bash
make run-only

```

系统会弹出两个终端标签：

* **Normal World** (非安全世界)：运行 Linux 内核。
* **Secure World** (安全世界)：显示 OP-TEE 运行日志。

### 5.2 测试

在 **Normal World** 终端登录 `root` 用户后，运行：

```bash
xtest

```

若所有测试用例显示 `PASSED`，则环境搭建成功。

---

## 常见问题排查 (FAQ)

* **报错 `qemu-system-aarch64-not-found**`：全量编译（Step 4.2）未成功或中途停止，请确保 `make` 完整执行且无 Error。
* **输入命令无反应**：QEMU 后台进程未启动。请检查编译日志中是否有固件缺失。
* **磁盘满报错 (Error 28)**：请务必执行 Step 2 中的 LVM 扩容步骤。
