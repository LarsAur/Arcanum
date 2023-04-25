
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

all: setup $(BUILDDIR)/$(PROJECT).exe

run: $(BUILDDIR)/$(PROJECT).exe
	./$^

clean: 
	-rmdir $(BUILDDIR) /s /q

$(BUILDDIR):
	-mkdir $(BUILDDIR)

# Create all build subfolders
setup: $(BUILDDIR)
	@- $(foreach folder,$(BUILDSUBDIRS),mkdir $(folder)) 

$(BUILDDIR)/%.o: %.cpp $(HEADERS)
	$(CC) $(CFLAGS) $(LDFLAGS) -I$(HEADERDIR) -I$(dir $<) -c $< -o $@

$(BUILDDIR)/$(PROJECT).exe: $(OBJECTS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJECTS) -o $@