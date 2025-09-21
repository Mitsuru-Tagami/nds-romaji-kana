LDFLAGS = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) -L/opt/devkitpro/libnds/lib -L/opt/devkitpro/libfat/libLDFLAGS = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) -L/opt/devkitpro/libnds/lib -L/opt/devkitpro/libfat/lib#---------------------------------------------------------------------------------
# NDS Makefile
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

                    $(foreach dir,$(LIBDIRS),-I$(dir)/include) \

TARGET := kana_ime_test
BUILD := build

CFLAGS := -g -Wall -O2 -march=armv5te -mtune=arm946e-s -fomit-frame-pointer -ffast-math -mthumb -mthumb-interwork -DENABLE_DEBUG_LOG -I. -I../cleanup_archive -I/opt/devkitpro/libnds/include -DARM9

all: $(BUILD)/$(TARGET).nds $(BUILD)/libkanaime.a

$(BUILD)/main.o: main.c | $(BUILD) $(BUILD)/libkanaime.a
	@echo "COMPILING $(notdir $<)"
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD)/%.o: %.c | $(BUILD)
	@echo "COMPILING $(notdir $<)"
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD):
	@echo "CREATING $(BUILD) directory"
	mkdir -p $(BUILD)

$(BUILD)/$(TARGET).elf: $(BUILD)/main.o $(BUILD)/libkanaime.a
	@echo "LINKING $(notdir $@)"
	arm-none-eabi-gcc -specs=ds_arm9.specs -g -mthumb -mthumb-interwork -Wl,-Map,$(notdir $@).map -L/opt/devkitpro/libnds/lib -L/opt/devkitpro/libfat/lib -o $@ $^ -lnds9 -lfat

ELF2NDS := ndstool -c
$(BUILD)/$(TARGET).nds: $(BUILD)/$(TARGET).elf
	@echo "CREATING NDS $(notdir $@)"
	$(ELF2NDS) $@ -9 $<

LIB_SOURCES := $(filter-out main.c,$(notdir $(wildcard *.c)))
LIB_OBJECTS := $(addprefix $(BUILD)/,$(LIB_SOURCES:.c=.o))

$(BUILD)/libkanaime.a: $(LIB_OBJECTS)
	@echo "ARCHIVING $(notdir $@)"
	arm-none-eabi-gcc-ar -rcs $@ $(LIB_OBJECTS)

clean:
	@echo "CLEAN $(BUILD)"
	rm -rf $(BUILD) $(TARGET).nds $(TARGET).elf $(TARGET).gba $(TARGET).bin

.PHONY: all clean library



clean:
	@echo "CLEAN $(BUILD)"
	$(QUIET) rm -rf $(BUILD) $(TARGET).nds $(TARGET).elf $(TARGET).gba $(TARGET).bin

#---------------------------------------------------------------------------------

DEPENDS    := $(OFILES:.o=.d)

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).nds    :     $(OUTPUT).elf
$(OUTPUT).elf    :    $(OFILES)

#---------------------------------------------------------------------------------
%.o    :    %.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	$(bin2o)


-include $(DEPENDS)

#---------------------------------------------------------------------------------------
#---------------------------------------------------------------------------------------