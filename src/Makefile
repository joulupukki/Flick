# Project Name
TARGET = flick

PLATFORM ?= funbox

# Uncomment to use LGPL (like ReverbSc, etc.)
#USE_DAISYSP_LGPL=1

# Base sources (common to all platforms)
CPP_SOURCES = flick.cpp delay_effect.cpp tremolo_effect.cpp plate_reverb.cpp hall_reverb.cpp spring_reverb.cpp flick_oscillator.cpp daisy_hardware.cpp parameter_capture.cpp

# CMSIS-DSP sources for FFT-based convolution (partitioned overlap-save for IR processing)
C_SOURCES = $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_rfft_fast_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_rfft_fast_init_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_cfft_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_cfft_init_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_cfft_radix8_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/TransformFunctions/arm_bitreversal2.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/CommonTables/arm_common_tables.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/CommonTables/arm_const_structs.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/ComplexMathFunctions/arm_cmplx_mult_cmplx_f32.c \
            $(LIBDAISY_DIR)/Drivers/CMSIS-DSP/Source/BasicMathFunctions/arm_add_f32.c

# Base includes
C_INCLUDES = -I.. -I./PlateauNEVersio

# Platform guard
ifneq ($(PLATFORM),funbox)
ifneq ($(PLATFORM),hothouse)
    $(error Invalid PLATFORM value: $(PLATFORM). Must be funbox or hothouse)
endif
endif

# Pass PLATFORM to compiler as a define
C_DEFS += -DPLATFORM=$(PLATFORM)
C_DEFS += -DPLATFORM_$(PLATFORM)=1
C_DEFS += -DUSE_ARM_DSP

# Add PlateauNEVersio sources
CPP_SOURCES += PlateauNEVersio/utilities/Utilities.cpp
CPP_SOURCES += PlateauNEVersio/dsp/filters/OnePoleFilters.cpp
CPP_SOURCES += PlateauNEVersio/dsp/delays/InterpDelay.cpp
CPP_SOURCES += PlateauNEVersio/Dattorro.cpp

# Library Locations
LIBDAISY_DIR = ../libDaisy
DAISYSP_DIR = ../DaisySP

# Optimise for size rather than speed (-O2 is the libDaisy default)
OPT = -Os

# Use Daisy bootloader: application is written to QSPI flash via DFU,
# then copied to SRAM at boot. This enables storing IR data in QSPI.
APP_TYPE = BOOT_SRAM

# Use the 2000ms grace period bootloader so that firmware can be
# reflashed by pressing RESET and running make program-dfu within 2.5s
BOOT_BIN = $(SYSTEM_FILES_DIR)/dsy_bootloader_v6_3-intdfu-2000ms.bin

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

# Override libDaisy's `program` target (which errors out for BOOT_SRAM)
# to flash firmware directly to QSPI via the STLINK debug probe.
.PHONY: program program-boot-probe flash help update-irs

program:
	@echo "Flashing firmware to QSPI via debug probe..."
	$(OCD) -s $(OCD_DIR) \
		-f $(PGM_DEVICE) \
		-c "set QUADSPI 1" \
		-f target/$(CHIPSET).cfg \
		-f openocd_daisy_qspi.cfg \
		-c "init" \
		-c "reset init" \
		-c "program $(BUILD_DIR)/$(TARGET_BIN) verify reset exit $(QSPI_ADDRESS)"

# Flash the Daisy bootloader to internal flash via debug probe.
program-boot-probe:
	@echo "Flashing Daisy bootloader via debug probe..."
	$(OCD) -s $(OCD_DIR) $(OCDFLAGS) \
		-c "program $(BOOT_BIN) verify reset exit $(INTERNAL_ADDRESS)"

# Alias for program-dfu
flash: program-dfu

# IR data generation
PYTHON ?= python3

update-irs:
	$(PYTHON) ../tools/wav_to_ir_header.py "../src/irs/Reverb_Dwell_10_Tone_10_48k.wav" -o ir_data.h --max-length 1000

help:
	@echo "Flick Build System"
	@echo "=================="
	@echo ""
	@echo "First-time setup (flash Daisy bootloader):"
	@echo "  Option A - USB only:"
	@echo "    1. Connect Daisy Seed via USB"
	@echo "    2. Hold BOOT button and press RESET, release both"
	@echo "    3. Run 'make program-boot'"
	@echo "  Option B - Debug probe (no buttons needed):"
	@echo "    1. Connect STLINK debug probe"
	@echo "    2. Run 'make program-boot-probe'"
	@echo ""
	@echo "Flashing firmware:"
	@echo "  Option A - USB only:"
	@echo "    1. Enter Daisy bootloader (press RESET, or hold both footswitches 2s)"
	@echo "    2. Run 'make program-dfu' (or 'make flash') within 2.5 seconds"
	@echo "  Option B - Debug probe (no USB or buttons needed):"
	@echo "    1. Connect STLINK debug probe (power via 9V or USB)"
	@echo "    2. Run 'make program'"
	@echo ""
	@echo "Common targets:"
	@echo "  make                     - Build for Funbox (default)"
	@echo "  make PLATFORM=hothouse   - Build for HotHouse"
	@echo "  make update-irs          - Rebuild IR header from WAV files"
	@echo "  make clean               - Clean build files"
	@echo "  make program-boot        - Flash bootloader via USB DFU (first-time)"
	@echo "  make program-dfu         - Flash firmware via USB DFU"
	@echo "  make flash               - Alias for program-dfu"
	@echo "  make program             - Flash firmware via debug probe (QSPI direct)"
	@echo "  make program-boot-probe  - Flash bootloader via debug probe"
