K=kernel
U=user
SRC=src

# ===== 并行编译配置 =====
# 默认使用所有可用 CPU 核心进行并行编译
NPROC := $(shell nproc)
MAKEFLAGS += -j$(NPROC)

# ===== 路径定义 =====
SRC_DIRS := boot devs lib mm proc sync syscall trap
BUILD_DIR := build

# ===== 文件收集规则 =====
# 收集 src 目录下各子目录的源文件
SRCS := $(shell find $(SRC) -type f \( -name "*.c" -o -name "*.S" \))

$(info === SRCS collected ===)
$(info $(SRCS))

# 将源文件路径转换为目标文件路径
OBJS := $(patsubst $(SRC)/%.c, $(BUILD_DIR)/%.o, $(filter %.c, $(SRCS)))
OBJS += $(patsubst $(SRC)/%.S, $(BUILD_DIR)/%.o, $(filter %.S, $(SRCS)))

# 设置 entry.o 作为特殊的入口目标文件
ENTRY_OBJ := $(BUILD_DIR)/boot/entry.o
OBJS_NO_ENTRY := $(filter-out $(ENTRY_OBJ), $(OBJS))
DEPS := $(OBJS:.o=.d)

# riscv64-unknown-elf- or riscv64-linux-gnu-
# perhaps in /opt/riscv/bin
#TOOLPREFIX = 

# Try to infer the correct TOOLPREFIX if not set
ifndef TOOLPREFIX
TOOLPREFIX := $(shell if riscv64-unknown-elf-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-elf-'; \
	elif riscv64-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-linux-gnu-'; \
	elif riscv64-unknown-linux-gnu-objdump -i 2>&1 | grep 'elf64-big' >/dev/null 2>&1; \
	then echo 'riscv64-unknown-linux-gnu-'; \
	else echo "***" 1>&2; \
	echo "*** Error: Couldn't find a riscv64 version of GCC/binutils." 1>&2; \
	echo "*** To turn off this error, run 'gmake TOOLPREFIX= ...'." 1>&2; \
	echo "***" 1>&2; exit 1; fi)
endif

QEMU = qemu-system-riscv64

CC = $(TOOLPREFIX)gcc
AS = $(TOOLPREFIX)gas
LD = $(TOOLPREFIX)ld
OBJCOPY = $(TOOLPREFIX)objcopy
OBJDUMP = $(TOOLPREFIX)objdump

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -MD
CFLAGS += -mcmodel=medany
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I. -I$(SRC)
CFLAGS += $(shell $(CC) -fno-stack-protector -E -x c /dev/null >/dev/null 2>&1 && echo -fno-stack-protector)

# 包含头文件路径：添加各个源代码子目录
INCLUDES := -I$(SRC) $(foreach dir,$(SRC_DIRS),-I$(SRC)/$(dir))

# Disable PIE when possible (for Ubuntu 16.10 toolchain)
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]no-pie'),)
CFLAGS += -fno-pie -no-pie
endif
ifneq ($(shell $(CC) -dumpspecs 2>/dev/null | grep -e '[^f]nopie'),)
CFLAGS += -fno-pie -nopie
endif

LDFLAGS = -z max-page-size=4096

# ===== 创建构建目录 =====
dirs:
	@mkdir -p $(BUILD_DIR)
	@for dir in $(SRC_DIRS); do mkdir -p $(BUILD_DIR)/$$dir; done

# ===== 编译规则 =====
$(BUILD_DIR)/%.o: $(SRC)/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC)/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(INCLUDES) -MMD -MP -c $< -o $@

$K/kernel: dirs $(ENTRY_OBJ) $(OBJS_NO_ENTRY) $(SRC)/linker/kernel.ld $U/initcode.bin
	@mkdir -p $K
	$(LD) $(LDFLAGS) -T $(SRC)/linker/kernel.ld -o $K/kernel $(ENTRY_OBJ) $(OBJS_NO_ENTRY)
	$(OBJDUMP) -S $K/kernel > $K/kernel.asm
	$(OBJDUMP) -t $K/kernel | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $K/kernel.sym

# ===== User 程序编译规则 =====
# 生成系统调用汇编文件
$U/usys.S: $U/usys.pl
	perl $U/usys.pl > $U/usys.S

# 编译系统调用汇编文件
$U/usys.o: $U/usys.S
	$(CC) $(CFLAGS) -c -o $U/usys.o $U/usys.S

# 编译 initcode.c 为 ELF 文件
$U/initcode.o: $U/initcode.c $U/user.h
	$(CC) $(CFLAGS) -march=rv64g -nostdinc -I. -I$(SRC) -c $U/initcode.c -o $U/initcode.o

# 链接生成 initcode ELF 文件
$U/initcode: $U/initcode.o $U/usys.o $U/user-riscv.ld
	$(LD) $(LDFLAGS) -T $U/user-riscv.ld -o $U/initcode $U/initcode.o $U/usys.o
	$(OBJDUMP) -S $U/initcode > $U/initcode.asm
	$(OBJDUMP) -t $U/initcode | sed '1,/SYMBOL TABLE/d; s/ .* / /; /^$$/d' > $U/initcode.sym

# 从 ELF 文件生成二进制文件
$U/initcode.bin: $U/initcode
	$(OBJCOPY) -S -O binary $< $@

tags: $(OBJS) _init
	etags *.S *.c

# ===== 磁盘文件系统构建工具 (已注释) =====
# mkfs/mkfs: mkfs/mkfs.c $(SRC)/fs/fs.h $(SRC)/param.h
# 	gcc -Werror -Wall -I. -I$(SRC) -o mkfs/mkfs mkfs/mkfs.c

# Prevent deletion of intermediate files, e.g. cat.o, after first build, so
# that disk image changes after first build are persistent until clean.  More
# details:
# http://www.gnu.org/software/make/manual/html_node/Chained-Rules.html
.PRECIOUS: %.o

# ===== 磁盘镜像构建 (已注释) =====
# fs.img: mkfs/mkfs README $(UPROGS)
# 	mkfs/mkfs fs.img README $(UPROGS)

-include $(DEPS)

clean: 
	rm -f *.tex *.dvi *.idx *.aux *.log *.ind *.ilg \
	$K/kernel fs.img \
	mkfs/mkfs .gdbinit
	rm -f $U/initcode $U/initcode.o $U/initcode.asm $U/initcode.sym $U/initcode.d $U/initcode.bin
	rm -f $U/usys.S $U/usys.o $U/usys.d
	rm -rf $(BUILD_DIR)

# try to generate a unique GDB port
GDBPORT = $(shell expr `id -u` % 5000 + 25000)
# QEMU's gdb stub command line changed in 0.11
QEMUGDB = $(shell if $(QEMU) -help | grep -q '^-gdb'; \
	then echo "-gdb tcp::$(GDBPORT)"; \
	else echo "-s -p $(GDBPORT)"; fi)
ifndef CPUS
CPUS := 1
endif

QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS += -global virtio-mmio.force-legacy=false
# 注释：磁盘相关的 QEMU 选项 (已注释)
# QEMUOPTS += -drive file=fs.img,if=none,format=raw,id=x0
# QEMUOPTS += -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

# 注释：移除了对 fs.img 的依赖
qemu: $K/kernel
	$(QEMU) $(QEMUOPTS)

.gdbinit: .gdbinit.tmpl-riscv
	sed "s/:1234/:$(GDBPORT)/" < $^ > $@

# 注释：移除了对 fs.img 的依赖
qemu-gdb: $K/kernel .gdbinit
	@echo "*** Now run 'gdb' in another window." 1>&2
	$(QEMU) $(QEMUOPTS) -S $(QEMUGDB)

