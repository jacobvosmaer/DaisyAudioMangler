# Project Name
TARGET = Mangler

# Sources
CPP_SOURCES = Mangler.cpp
C_SOURCES = ../audio.c

# Library Locations
LIBDAISY_DIR = ../libDaisy

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

format: *.cpp
	clang-format -i *.cpp
