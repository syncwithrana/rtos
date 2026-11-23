CC          = arm-none-eabi-gcc
GDB         = arm-none-eabi-gdb
QEMU        = qemu-system-arm
QEMU_MACH   = mps2-an385
LED_ADDR    = 0x40028000
LED_MAP     = G:0,R:1,B:2,Y:3

CFLAGS  = -mcpu=cortex-m3 -mthumb -ffreestanding -O0 -g -Wall -I. -Ibsp -Iplatform
LDFLAGS = -T platform/linker.ld -nostartfiles -nostdlib

SRCS    = platform/startup.c miros.c bsp/bsp.c main.c
BUILD_DIR = build
TARGET  = $(BUILD_DIR)/rtos.elf

all: $(TARGET)

$(TARGET): $(SRCS) platform/linker.ld platform/mps2_an385.h bsp/bsp.h miros.h
	mkdir -p $(BUILD_DIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $(TARGET)

inspect: $(TARGET)
	-killall -9 $(QEMU) $(GDB) 2>/dev/null || true
	python3 debug/led_monitor.py \
		--elf $(TARGET) \
		--machine $(QEMU_MACH) \
		--qemu $(QEMU) \
		--gdb $(GDB) \
		--addr $(LED_ADDR) \
		--leds $(LED_MAP)

clean:
	rm -rf $(BUILD_DIR)

kill:
	-killall -9 $(QEMU) 2>/dev/null || true
	-killall -9 $(GDB) 2>/dev/null || true

.PHONY: all inspect clean kill
