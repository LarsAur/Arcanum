
PROJECT ?= Arcanum
SOURCEDIR = src
HEADERDIR = src
BUILDDIR = build
NNUE = nn-04cf2b4ed1da.nnue

DEFINES += -DIS_64BIT
DEFINES += -DUSE_AVX2 -mavx2
DEFINES += -DUSE_BMI -mbmi
DEFINES += -DUSE_BMI2 -mbmi2
DEFINES += -DUSE_POPCNT -mpopcnt
DEFINES += -DUSE_LZCNT -mlzcnt

CC = g++
override CFLAGS += -std=c++17 -O3 -Wall -mtune=native $(DEFINES)
LDFLAGS = -lm -lstdc++

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SOURCES := $(call rwildcard, $(SOURCEDIR)/, %.cpp)							     # Recursive search all files in source directory
HEADERS := $(filter %.hpp, $(SOURCES))										     # Find all headers
SOURCES := $(filter-out %.hpp, $(SOURCES))									     # Filter out header files
SOURCES := $(filter-out %/, $(SOURCES))											 # Filter out folder
OBJECTS := $(addprefix $(BUILDDIR)/,$(SOURCES:%.cpp=%.o)) 						 # Create list of all object files

all: $(BUILDDIR)/$(PROJECT).exe

.PHONY: uci run test perf
uci: $(BUILDDIR)/$(PROJECT).exe
	./$^

test: $(BUILDDIR)/$(PROJECT).exe
	./$^ --draw-test --capture-test --zobrist-test --symeval-test --perft-test

perf: $(BUILDDIR)/$(PROJECT).exe
	./$^ --search-perf --engine-perf

clean:
	-rmdir $(BUILDDIR) /s /q

clean-logs:
	del *.log
	cd snapshots & del *.log
	cd build & del *.log

$(BUILDDIR):
	mkdir $(BUILDDIR)
	cd $(BUILDDIR) && mkdir src
	cd $(BUILDDIR)/src && mkdir nnue

$(BUILDDIR)/$(NNUE):
ifeq ($(OS),Windows_NT)
	-copy /b $(NNUE) /b $(BUILDDIR)/$(NNUE)
else
	-cp $(NNUE) $(BUILDDIR)
endif

$(BUILDDIR)/%.o: %.cpp $(HEADERS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -I$(HEADERDIR) -I$(dir $<) -c $< -o $@

$(BUILDDIR)/$(PROJECT).exe: $(OBJECTS) | $(BUILDDIR)/$(NNUE)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@