# Project Name
TARGET = Mangler

# Sources
CPP_SOURCES = Mangler.cpp

# Library Locations
LIBDAISY_DIR = ./libDaisy

CFLAGS = -Werror -pedantic

# Core location, and generic Makefile.
SYSTEM_FILES_DIR = $(LIBDAISY_DIR)/core
include $(SYSTEM_FILES_DIR)/Makefile

format: *.cpp
	clang-format -i *.cpp
