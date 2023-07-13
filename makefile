
PROJECT = chessEngine2
SOURCEDIR = src
HEADERDIR = src	
BUILDDIR = build

CC = g++
CFLAGS = -Wall -O3 -mbmi -mpopcnt -mtune=native
LDFLAGS = -lm -lstdc++ 

rwildcard=$(wildcard $1$2) $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2))
SOURCES := $(call rwildcard, $(SOURCEDIR)/, %.cpp)							     # Recursive search all files in source directory
HEADERS := $(filter %.hpp, $(SOURCES))										     # Find all headers
SOURCES := $(filter-out %.hpp, $(SOURCES))									     # Filter out header files
BUILDSUBDIRS := $(subst /,\,$(addprefix $(BUILDDIR)/, $(filter %/, $(SOURCES)))) # Find all source folder names
SOURCES := $(filter-out %/, $(SOURCES))											 # Filter out folder
OBJECTS := $(addprefix $(BUILDDIR)/,$(SOURCES:%.cpp=%.o))						 # Create list of all object files

DISABLE_LOG_FLAGS = -DDISABLE_CE2_DEBUG -DDISABLE_CE2_LOG -DDISABLE_CE2_WARNING -DDISABLE_CE2_ERROR -DDISABLE_CE2_SUCCESS

all: setup $(BUILDDIR)/$(PROJECT).exe

.PHONY: uci run test perf
uci: $(BUILDDIR)/$(PROJECT).exe
	./$^

run: $(BUILDDIR)/$(PROJECT).exe
	./$^ --nouci

test: $(BUILDDIR)/$(PROJECT).exe
	./$^ --nouci --test

perf: $(BUILDDIR)/$(PROJECT).exe
	./$^ --nouci --perf

clean: 
	-rmdir $(BUILDDIR) /s /q

$(BUILDDIR):
	-mkdir $(BUILDDIR)

# Create all build subsfolders
setup: $(BUILDDIR)
	@- $(foreach folder,$(BUILDSUBDIRS),mkdir $(folder)) 

$(BUILDDIR)/%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) -I$(HEADERDIR) -I$(dir $<) -c $< -o $@

$(BUILDDIR)/$(PROJECT).exe: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@