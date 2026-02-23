# ~/tee_lab/host/Makefile

CC := gcc

# 直接用源码目录中的头文件 + 构建产物目录中的库
OPTEE_CLIENT_SRC := /home/shannon/optee-qemu/optee_client
OPTEE_CLIENT_LIB := /home/shannon/optee-qemu/out-br/target/usr/lib

CFLAGS := -Wall -O2 \
	-I../include \
	-I$(OPTEE_CLIENT_SRC)/libteec/include

LDFLAGS := -L$(OPTEE_CLIENT_LIB)
LDLIBS := -lteec

TARGET := secure_hmac_ca
SRCS := main.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET) *.o

print:
	@echo "CFLAGS=$(CFLAGS)"
	@echo "LDFLAGS=$(LDFLAGS)"
	@echo "LDLIBS=$(LDLIBS)"
