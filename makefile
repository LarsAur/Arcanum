
PROJECT ?= Arcanum
SOURCEDIR = src
HEADERDIR = src
BUILDDIR = build
NNUE = arcanum1.fnnue

DEFINES += -DIS_64BIT
DEFINES += -DUSE_AVX2 -mavx2 -mfma
DEFINES += -DUSE_BMI -mbmi
DEFINES += -DUSE_BMI2 -mbmi2
DEFINES += -DUSE_POPCNT -mpopcnt
DEFINES += -DUSE_LZCNT -mlzcnt

CC = g++
override CFLAGS += -std=c++17 -O3 -Wall $(DEFINES)
LDFLAGS = -lpthread -lm -lstdc++ --static

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SOURCES := $(call rwildcard, $(SOURCEDIR)/, %.cpp)							     # Recursive search all files in source directory
SOURCES := $(filter-out %/LICENSE, $(SOURCES))									 # LICENSE
HEADERS := $(filter %.hpp, $(SOURCES))										     # Find all headers
SOURCES := $(filter-out %.hpp, $(SOURCES))									     # Filter out header files
SOURCES := $(filter-out %/, $(SOURCES))											 # Filter out folder
OBJECTS := $(addprefix $(BUILDDIR)/,$(SOURCES:%.cpp=%.o)) 						 # Create list of all object files

all: $(BUILDDIR)/$(PROJECT).exe

.PHONY: uci run test perf
uci: $(BUILDDIR)/$(PROJECT).exe
	./$^

test: $(BUILDDIR)/$(PROJECT).exe
	./$^ --see-test --draw-test --capture-test --zobrist-test --perft-test

perf: $(BUILDDIR)/$(PROJECT).exe
	./$^ --search-perf --engine-perf

clean:
ifeq ($(OS),Windows_NT)
	-rmdir $(BUILDDIR) /s /q
else
	-rm -rf $(BUILDDIR)
endif

clean-logs:
	del *.log
	cd snapshots & del *.log
	cd build & del *.log

$(BUILDDIR):
	mkdir $(BUILDDIR)
	cd $(BUILDDIR) && mkdir src
	cd $(BUILDDIR)/src && mkdir nnue
	cd $(BUILDDIR)/src && mkdir tuning
	cd $(BUILDDIR)/src && mkdir syzygy

$(BUILDDIR)/$(NNUE):
ifeq ($(OS),Windows_NT)
	-copy /b $(NNUE) /b $(BUILDDIR)/$(NNUE)
else
	-cp $(NNUE) $(BUILDDIR)
endif

$(BUILDDIR)/%.o: %.cpp $(HEADERS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) -I$(HEADERDIR) -I$(dir $<) -c $< -o $@

$(BUILDDIR)/$(PROJECT).exe: $(OBJECTS) | $(BUILDDIR)/$(NNUE)
	$(CC) $(OBJECTS) $(CFLAGS) $(LDFLAGS) -o $@