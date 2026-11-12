CC      = arm-none-eabi-gcc
GDB     = arm-none-eabi-gdb
QEMU    = qemu-system-arm

CFLAGS  = -mcpu=cortex-m3 -mthumb -ffreestanding -O0 -g -Wall -I. -Ibsp -Iplatform
LDFLAGS = -T platform/linker.ld -nostartfiles -nostdlib

SRCS    = platform/startup.c main.c bsp/bsp.c
BUILD_DIR = build
TARGET  = $(BUILD_DIR)/rtos.elf

all: $(TARGET)

$(TARGET): $(SRCS) platform/linker.ld platform/TM4C123GH6PM.h bsp/bsp.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $(TARGET)

run: $(TARGET)
	$(QEMU) -M lm3s6965evb -nographic -kernel $(TARGET)

debug: $(TARGET)
	$(QEMU) -M lm3s6965evb -nographic -s -S -kernel $(TARGET)

inspect: $(TARGET)
	python3 debug/led_monitor.py --spawn-qemu

clean:
	rm -rf $(BUILD_DIR)

kill:
	-killall -9 qemu-system-arm 2>/dev/null || true
	-killall -9 arm-none-eabi-gdb 2>/dev/null || true
	-killall -9 gdb 2>/dev/null || true

.PHONY: all run debug inspect clean kill
