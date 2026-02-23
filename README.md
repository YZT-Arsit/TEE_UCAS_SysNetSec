# OP-TEE 实验环境搭建指南

本指南旨在帮助学生搭建基于 **QEMU** 的 **OP-TEE (Open Portable Trusted Execution Environment)** 仿真开发环境。

---

## 0. 硬件与系统要求

* **宿主机**: Windows 或 macOS (建议 Apple Silicon M1/M2/M3)。
* **虚拟机**: Ubuntu 24.04 LTS。
* **磁盘空间**: 虚拟机硬盘容量需分配 **至少 50GB**。
* **内存**: 建议分配 **至少 8GB** 内存以防止编译时内存溢出。

---

## 1. 基础依赖安装

在 Ubuntu 24.04 终端执行以下命令，安装编译所需的依赖包。

```bash
sudo apt-get update
sudo apt-get install -y adb fastboot autoconf automake bc bison build-essential \
    ccache cscope cppcheck curl device-tree-compiler expect flex ftp gdisk \
    libattr1-dev libcap-dev libfdt-dev libftdi-dev libglib2.0-dev libhidapi-dev \
    libncurses-dev libpixman-1-dev libssl-dev libtool make mtools \
    netcat-openbsd python3-cryptography python3-pip python3-pyelftools \
    python3-serial python3-yaml rsync unzip uuid-dev xdg-utils xterm zlib1g-dev \
    libgnutls28-dev \
    build-essential git curl wget unzip rsync \
    python3 python3-pip python3-venv \
    device-tree-compiler flex bison bc cpio \
    cmake ninja-build pkg-config \
    libssl-dev libgnutls28-dev libncurses-dev libelf-dev \
    qemu-system-arm qemu-system-misc qemu-utils \
    gcc-aarch64-linux-gnu gcc-arm-linux-gnueabihf

```

---

## 2. 源码同步

### 2.1 安装 Repo 工具

```bash
mkdir -p ~/bin
curl https://storage.googleapis.com/git-repo-downloads/repo > ~/bin/repo
chmod a+x ~/bin/repo
export PATH=$PATH:~/bin

```

### 2.2 配置 Git 身份

```bash
git config --global user.email "you@example.com"
git config --global user.name "Your Name"

```

### 2.3 初始化与同步

```bash
mkdir -p ~/optee-qemu && cd ~/optee-qemu
repo init -u https://github.com/OP-TEE/manifest.git -m qemu_v8.xml
repo sync -j$(nproc)

```

---

## 3. 构建过程

### 3.1 安装工具链

```bash
cd ~/optee-qemu/build
make -j$(nproc) toolchains

```

### 3.2 全量编译

为避免 `ccache` 参数冲突导致的 `invalid option -- 'f'` 报错，建议显式禁用缓存进行全量编译。

```bash
make -j$(nproc) CCACHE=

```

---

## 4. 运行与验证

### 4.1 启动仿真器

```bash
make run-only

```

系统会弹出两个终端标签：

* **Normal World** ：运行 Linux 内核。
* **Secure World** ：显示 OP-TEE 运行日志。

### 4.2 测试

在 **Normal World** 终端登录 `root` 用户后，运行：

```bash
xtest

```

若所有测试用例显示 `PASSED`，则环境搭建成功。

---
