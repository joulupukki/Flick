# Project Name
TARGET = flick

PLATFORM ?= funbox

# Uncomment to use LGPL (like ReverbSc, etc.)
#USE_DAISYSP_LGPL=1

# Base sources (common to all platforms)
CPP_SOURCES = flick.cpp delay_effect.cpp tremolo_effect.cpp plate_reverb.cpp hall_reverb.cpp spring_reverb.cpp flick_oscillator.cpp daisy_hardware.cpp parameter_capture.cpp

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

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
