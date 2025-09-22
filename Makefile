LDFLAGS = -specs=ds_arm9.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map) -L/opt/devkitpro/libnds/lib -L/opt/devkitpro/libfat/lib -lfat
#---------------------------------------------------------------------------------
# NDS Makefile
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

include $(DEVKITARM)/ds_rules

NDS_SKK_DIR := NDS_SKK

TARGET := nds_skk_ime
BUILD := build

CFLAGS := -g -Wall -O2 -march=armv5te -mtune=arm946e-s -fomit-frame-pointer -ffast-math -mthumb -mthumb-interwork -DENABLE_DEBUG_LOG -I. -I$(NDS_SKK_DIR) -I../cleanup_archive -I/opt/devkitpro/libnds/include -DARM9
CXXFLAGS := $(CFLAGS) -fno-exceptions -fno-rtti

all: $(BUILD)/$(TARGET).nds

SOURCES := $(NDS_SKK_DIR)/main.c \
           $(NDS_SKK_DIR)/kana_ime.cpp \
           $(NDS_SKK_DIR)/skk.cpp \
           $(NDS_SKK_DIR)/JString.cpp \
           draw_font.c \
           mplus_font_10x10.c \
           mplus_font_10x10alpha.c

OBJECTS := $(patsubst $(NDS_SKK_DIR)/%.c,$(BUILD)/%.o,$(filter $(NDS_SKK_DIR)/%.c,$(SOURCES))) \
           $(patsubst $(NDS_SKK_DIR)/%.cpp,$(BUILD)/%.o,$(filter $(NDS_SKK_DIR)/%.cpp,$(SOURCES))) \
           $(patsubst %.c,$(BUILD)/%.o,$(filter-out $(NDS_SKK_DIR)/%.c,$(filter %.c,$(SOURCES))))

# Rule for C files in the current directory
$(BUILD)/%.o: %.c | $(BUILD)
	@echo "COMPILING $(notdir $<)"
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Rule for C files
$(BUILD)/%.o: $(NDS_SKK_DIR)/%.c | $(BUILD)
	@echo "COMPILING $(notdir $<)"
	$(CC) $(CFLAGS) $(INCLUDE) -c $< -o $@

# Rule for C++ files
$(BUILD)/%.o: $(NDS_SKK_DIR)/%.cpp | $(BUILD)
	@echo "COMPILING $(notdir $<)"
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

$(BUILD):
	@echo "CREATING $(BUILD) directory"
	mkdir -p $(BUILD)

$(BUILD)/$(TARGET).elf: $(OBJECTS)
	@echo "LINKING $(notdir $@)"
	$(CXX) -specs=ds_arm9.specs -g -mthumb -mthumb-interwork -Wl,-Map,$(notdir $@).map -L/opt/devkitpro/libnds/lib -L/opt/devkitpro/libfat/lib -o $@ $^ -lnds9 -lfat

ELF2NDS := ndstool -c
$(BUILD)/$(TARGET).nds: $(BUILD)/$(TARGET).elf
	@echo "CREATING NDS $(notdir $@)"
	$(ELF2NDS) $@ -9 $< 

clean:
	@echo "CLEAN $(BUILD)"
	rm -rf $(BUILD) $(TARGET).nds $(TARGET).elf $(TARGET).gba $(TARGET).bin

.PHONY: all clean

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