# Project Name
TARGET = flick

PLATFORM ?= funbox

# Uncomment to use LGPL (like ReverbSc, etc.)
#USE_DAISYSP_LGPL=1

# Base sources (common to all platforms)
CPP_SOURCES = flick.cpp flick_oscillator.cpp hall_reverb.cpp spring_reverb.cpp

# Base includes
C_INCLUDES = -I.. -I./PlateauNEVersio

# Platform-specific sources
ifeq ($(PLATFORM),funbox)
    CPP_SOURCES += ../platforms/funbox/funbox_gpl.cpp
    C_INCLUDES += -I../platforms/funbox
else ifeq ($(PLATFORM),hothouse)
    CPP_SOURCES += ../platforms/hothouse/hothouse.cpp
    C_INCLUDES += -I../platforms/hothouse
else
    $(error Invalid PLATFORM value: $(PLATFORM). Must be funbox or hothouse)
endif

# Pass PLATFORM to compiler as a define
CXXFLAGS += -DPLATFORM=$(PLATFORM)

# Add PlateauNEVersio sources
CPP_SOURCES += PlateauNEVersio/utilities/Utilities.cpp
CPP_SOURCES += PlateauNEVersio/dsp/filters/OnePoleFilters.cpp
CPP_SOURCES += PlateauNEVersio/dsp/delays/InterpDelay.cpp
CPP_SOURCES += PlateauNEVersio/Dattorro.cpp

# Library Locations
LIBDAISY_DIR = ../libDaisy
DAISYSP_DIR = ../DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile
