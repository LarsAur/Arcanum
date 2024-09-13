NAME ?= Arcanum
VERSION ?= dev_build
BUILDDIR ?= build
RELEASEDIR ?= releases
SOURCEDIR = src
HEADERDIR = src
BUILDDIR = build
NNUE = arcanum-net-v3.0.fnnue
CXX = g++

DEFINES += -DIS_64BIT
DEFINES += -DUSE_AVX2 -mavx2 -mfma
DEFINES += -DUSE_BMI -mbmi
DEFINES += -DUSE_BMI2 -mbmi2
DEFINES += -DUSE_POPCNT -mpopcnt
DEFINES += -DUSE_LZCNT -mlzcnt
DEFINES += -DARCANUM_VERSION=$(VERSION)

RELEASE_DEFINES += -DPRINT_TO_FILE
RELEASE_DEFINES += -DDISABLE_LOG
RELEASE_DEFINES += -DDISABLE_DEBUG

override CFLAGS += -std=c++17 -O3 -Wall $(DEFINES)
LDFLAGS = -lpthread -lm -lstdc++ --static

ifeq ($(OS),Windows_NT)
FILENAME = $(NAME).exe
else
FILENAME = $(NAME)
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
	./$^ --see-test --draw-test --capture-test --zobrist-test --perft-test

perf: $(BUILDDIR)/$(FILENAME)
	./$^ --search-perf --engine-perf

release: $(RELEASEDIR)
	make clean
	make $(BUILDDIR)/$(FILENAME) -j "CFLAGS=$(RELEASE_DEFINES)"
ifeq ($(OS),Windows_NT)
	copy ".\$(BUILDDIR)\$(FILENAME)" /b ".\$(RELEASEDIR)\$(FILENAME)" /b
	-copy ".\$(NNUE)" /b ".\$(RELEASEDIR)\$(NNUE)" /b
else
	cp $(BUILDDIR)/$(FILENAME) $(RELEASEDIR)/$(FILENAME)
	-cp $(NNUE) $(RELEASEDIR)
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
	cd $(BUILDDIR)/src && mkdir nnue
	cd $(BUILDDIR)/src && mkdir tuning
	cd $(BUILDDIR)/src && mkdir syzygy
	cd $(BUILDDIR)/src && mkdir uci

$(BUILDDIR)/$(NNUE):
ifeq ($(OS),Windows_NT)
	-copy /b $(NNUE) /b $(BUILDDIR)/$(NNUE)
else
	-cp $(NNUE) $(BUILDDIR)
endif

$(BUILDDIR)/%.o: %.cpp $(HEADERS) | $(BUILDDIR)
	$(CXX) $(CFLAGS) $(LDFLAGS) -I$(HEADERDIR) -I$(dir $<) -c $< -o $@

$(BUILDDIR)/$(FILENAME): $(OBJECTS) | $(BUILDDIR)/$(NNUE)
	$(CXX) $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@