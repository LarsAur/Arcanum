ENGINENAME ?= Arcanum
VERSION ?= dev_build
BUILDDIR ?= build
RELEASEDIR ?= releases
SOURCEDIR = src
HEADERDIR = src
DEFAULT_NNUE = arcanum-net-v6.0.fnnue
CXX = clang++

DEFINES += -DIS_64BIT
DEFINES += -DUSE_AVX2 -mavx2 -mfma
DEFINES += -DUSE_BMI -mbmi
DEFINES += -DUSE_BMI2 -mbmi2
DEFINES += -DUSE_POPCNT -mpopcnt
DEFINES += -DUSE_LZCNT -mlzcnt
DEFINES += -DARCANUM_VERSION=$(VERSION)
DEFINES += -DDEFAULT_NNUE=$(DEFAULT_NNUE)
DEFINES += -DENABLE_INCBIN # Remove to disable using incbin for DEFAULT_NNUE

RELEASE_DEFINES += -DLOG_FILE_NAME=$(ENGINENAME)
RELEASE_DEFINES += -DDISABLE_LOG
RELEASE_DEFINES += -DDISABLE_DEBUG

override CFLAGS += -std=c++17 -O3 -Wall -Wextra -pedantic $(DEFINES)
LDFLAGS = --static -lstdc++ -lm

ifeq ($(OS),Windows_NT)
FILENAME = $(ENGINENAME).exe
else
FILENAME = $(ENGINENAME)
endif

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SOURCES := $(call rwildcard, $(SOURCEDIR)/, %.cpp)          # Recursive search all files in source directory
SOURCES := $(filter-out %/LICENSE, $(SOURCES))              # LICENSE
HEADERS := $(filter %.hpp, $(SOURCES))                      # Find all headers
SOURCES := $(filter-out %.hpp, $(SOURCES))                  # Filter out header files
SOURCES := $(filter-out %/, $(SOURCES))                     # Filter out folder
OBJECTS := $(addprefix $(BUILDDIR)/,$(SOURCES:%.cpp=%.o))   # Create list of all object files

.PHONY: uci run test perf release

all: $(BUILDDIR)/$(FILENAME)

uci: $(BUILDDIR)/$(FILENAME)
	./$^

test: $(BUILDDIR)/$(FILENAME)
	./$^ --see-test --draw-test --capture-test --zobrist-test --perft-test --binpack-test

selfplay: $(BUILDDIR)/$(FILENAME)
	./$^ --selfplay-test

enginetest: $(BUILDDIR)/$(FILENAME)
	./$^ --engine-test

release: $(RELEASEDIR)
	make clean
	make $(BUILDDIR)/$(FILENAME) -j "CFLAGS=$(RELEASE_DEFINES)"
ifeq ($(OS),Windows_NT)
	copy ".\$(BUILDDIR)\$(FILENAME)" /b ".\$(RELEASEDIR)\$(FILENAME)" /b
	-copy ".\$(DEFAULT_NNUE)" /b ".\$(RELEASEDIR)\$(DEFAULT_NNUE)" /b
else
	cp $(BUILDDIR)/$(FILENAME) $(RELEASEDIR)/$(FILENAME)
	-cp $(DEFAULT_NNUE) $(RELEASEDIR)
endif

clean:
ifeq ($(OS),Windows_NT)
	-rmdir $(BUILDDIR) /s /q
else
	-rm -rf $(BUILDDIR)
endif

$(RELEASEDIR):
	mkdir $(RELEASEDIR)

$(BUILDDIR):
	mkdir $(BUILDDIR)
	cd $(BUILDDIR) && mkdir src
	cd $(BUILDDIR)/src && mkdir history
	cd $(BUILDDIR)/src && mkdir tuning
	cd $(BUILDDIR)/src && mkdir syzygy
	cd $(BUILDDIR)/src && mkdir uci
	cd $(BUILDDIR)/src && mkdir tests

$(BUILDDIR)/$(DEFAULT_NNUE):
ifeq ($(OS),Windows_NT)
	-copy .\$(DEFAULT_NNUE) /b .\$(BUILDDIR)\$(DEFAULT_NNUE) /b
else
	-cp $(DEFAULT_NNUE) $(BUILDDIR)
endif

$(BUILDDIR)/%.o: %.cpp $(HEADERS) | $(BUILDDIR)
	$(CXX) $(CFLAGS) -I$(HEADERDIR) -I$(dir $<) -c $< -o $@

$(BUILDDIR)/$(FILENAME): $(OBJECTS) | $(BUILDDIR)/$(DEFAULT_NNUE)
	$(CXX) $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@