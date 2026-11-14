ifndef PS4SDK
$(error PS4SDK is not set)
endif

LIBPS4 := $(PS4SDK)/libPS4
CC := gcc
OBJCOPY := objcopy
RM := rm
ODIR := build
SDIR := source
IDIRS := -I$(LIBPS4)/include
LDIRS := -L$(LIBPS4)
TARGET := rainbow_loader.bin
MAPFILE := rainbow_loader.map

CFLAGS := $(IDIRS) -Os -std=c11 -fno-builtin -nostartfiles -nostdlib \
	-Wall -Wextra -Werror -masm=intel -march=btver2 -mtune=btver2 \
	-m64 -mabi=sysv -mcmodel=small -fpie -ffunction-sections -fdata-sections
LFLAGS := $(LDIRS) -Xlinker -T $(LIBPS4)/linker.x \
	-Xlinker -Map=$(MAPFILE) -Wl,--build-id=none -Wl,--gc-sections \
	-Wl,-z,noexecstack
LIBS := -lPS4
CFILES := $(wildcard $(SDIR)/*.c)
OBJS := $(patsubst $(SDIR)/%.c,$(ODIR)/%.o,$(CFILES))

.PHONY: all clean
all: $(TARGET)

$(TARGET): $(ODIR) $(OBJS)
	$(CC) $(LIBPS4)/crt0.s $(OBJS) -o temp.t $(CFLAGS) $(LFLAGS) $(LIBS)
	$(OBJCOPY) -O binary temp.t $@
	$(RM) -f temp.t

$(ODIR)/%.o: $(SDIR)/%.c | $(ODIR)
	$(CC) -c -o $@ $< $(CFLAGS)

$(ODIR):
	mkdir -p $@

clean:
	$(RM) -rf $(TARGET) $(MAPFILE) $(ODIR) temp.t
