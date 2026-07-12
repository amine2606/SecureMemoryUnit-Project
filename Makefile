# ---------------------------------------
# CONFIGURATION BEGIN
# ---------------------------------------

# entry point for the program and target name
C_SRCS = src/rahmenprogramm.c
CPP_SRCS = src/run_simulation.cpp

# Object files
C_OBJS = $(C_SRCS:.c=.o)
CPP_OBJS = $(CPP_SRCS:.cpp=.o)

# assignment task file
HEADERS := include/secure_memory_unit.hpp include/run_simulation.hpp

# target name
TARGET := project

# Path to your systemc installation
SCPATH = $(SYSTEMC_HOME)


# Additional flags for the compiler
CXXFLAGS := -std=c++14  -I$(SCPATH)/include -L$(SCPATH)/lib -lsystemc -lm
CFLAGS := -std=c17

# ---------------------------------------
# CONFIGURATION END
# ---------------------------------------

# Determine if clang or gcc is available
CXX := $(shell command -v g++ || command -v clang++)
ifeq ($(strip $(CXX)),)
    $(error Neither clang++ nor g++ is available. Exiting.)
endif

CC := $(shell command -v gcc || command -v clang)
ifeq ($(strip $(CC)),)
    $(error Neither clang nor g is available. Exiting.)
endif

# Add rpath except for MacOS
UNAME_S := $(shell uname -s)

ifneq ($(UNAME_S), Darwin)
    CXXFLAGS += -Wl,-rpath=$(SCPATH)/lib
endif


# Default to release build for both app and library
all: debug

# Rule to compile .c files to .o files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Rule to compile .cpp files to .o files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Debug build
debug: CXXFLAGS += -g
debug: $(TARGET)

# Release build
release: CXXFLAGS += -O2
release: $(TARGET)

# Rule to link object files to executable
$(TARGET): $(C_OBJS) $(CPP_OBJS)
	$(CXX) $(CXXFLAGS) $(C_OBJS) $(CPP_OBJS) $(LDFLAGS) -o $(TARGET)

# clean up
clean:
	rm -f $(TARGET)
	rm -rf src/*.o
	rm -rf *.o

.PHONY: all debug release clean