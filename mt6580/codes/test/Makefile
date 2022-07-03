TARGET=mtkmodem
OBJS=\
	pre.o \
	sperd.o \
	com/xprintf.o

CFLAGS=-mcpu=cortex-r4f -O2 -Werror -mthumb -Icom -Wstringop-overflow=0
ASFLAGS=-mcpu=cortex-r4f
LDFLAGS=-mcpu=cortex-r4f -Tlink.ld -nostartfiles

CROSS_COMPILE=arm-none-eabi-
CC=$(CROSS_COMPILE)gcc
AS=$(CROSS_COMPILE)as
OBJDUMP=$(CROSS_COMPILE)objdump
OBJCOPY=$(CROSS_COMPILE)objcopy
SIZE=$(CROSS_COMPILE)size

all: $(TARGET) copy

clean:
	rm -f $(OBJS)
	rm -f "$(TARGET)"
	rm -f "$(TARGET).bin"
	rm -f "$(TARGET).lst"

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o "$@"
	$(SIZE) -A "$@"

copy: $(TARGET)
	$(OBJDUMP) -d "$<" > "$<.lst"
	$(OBJCOPY) -Obinary "$<" "$<.bin"
