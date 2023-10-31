# Project Name
TARGET = SimpleLed

# Sources
CPP_SOURCES = SimpleLed.cpp

# Library Locations
LIBDAISY_DIR = ./libDaisy
#DAISYSP_DIR = ./DaisySP

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

format: *.cpp
	clang-format -i *.cpp
