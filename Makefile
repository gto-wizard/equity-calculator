# Check the operating system
UNAME := $(shell uname)
ifeq ($(UNAME), Linux)
  ifeq ($(CC), cc)
    export CC := gcc-13
  endif
  ifeq ($(CXX), g++)
    export CXX := g++-13
  endif
else ifeq ($(UNAME), Darwin)
  export CC := /opt/homebrew/opt/llvm/bin/clang
  export CXX := /opt/homebrew/opt/llvm/bin/clang++
  export LDFLAGS := -L/opt/homebrew/opt/llvm/lib/c++ -L/opt/homebrew/opt/llvm/lib/unwind -lunwind
endif

# Set the default configuration preset
CONFIGURE_PRESET ?= default

.PHONY: all configure compile test clean

all: compile

build/.timestamp: CMakeLists.txt
	cmake --preset $(CONFIGURE_PRESET)

configure:
	cmake --preset $(CONFIGURE_PRESET) --fresh

# The extension file carries the interpreter and the platform in its name, so
# a build for one interpreter leaves the file for another in place and `pytest`
# can import the stale one. Clear them before every build.
compile: build/.timestamp
	rm -f eqcalc*.so eqcalc/*.so
	cmake --build build

# test: compile

clean:
	rm -rf build eqcalc*.so eqcalc/*.so
