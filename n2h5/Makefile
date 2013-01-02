######################################################################################
##
## Makefile for n2h5 nev/nsx to hdf5 conversion utility
##
##           Workfile: Makefile 
##           Archive: /n2h5/Makefile
##           Revision: 1 
##           Date: 11/1/12 1:00p 
##           Author: Ehsan
##
##
## Major targets:
##
## all       - make the entire project 
## debug     - make debug version
## clean     - clean up
##
######################################################################################

CC      := gcc
CXX     := g++

#==========================================================================
# Operating System
#==========================================================================
OS := $(shell echo %OS%)
ifeq ($(OS),Windows_NT)
WIN32 := 1
PWD := $(CURDIR)
else
OSVER := $(shell uname -r)
endif


#==========================================================================
# UTILITIES
#==========================================================================
MKPARENT := mkdir -p `dirname $(1)`
ECHO     := @echo
CP       := @cp


#==========================================================================
# CONSTANTS
#==========================================================================

# utility binary
ifdef WIN32
PROGRAM := n2h5$(DEBUG).exe
else
PROGRAM := n2h5$(DEBUG)
endif

# Test this with the -ffast-math option put in after all is working.
CFLAGS := -Wall -Wstrict-prototypes                           \
	-m32 \
	-I.                                                       \
	-I$(PWD)/../hdf5/include                                  \

ifdef DEBUG
BinDir  := debug
CFLAGS += -O0 -g3 -UNDEBUG -DDEBUG -DNO_AFX
else
BinDir  := release
CFLAGS += -O2 -DNDEBUG -UDEBUG -DNO_AFX
endif

# compatibility compilation flags
CFLAGS += $(COMPATCFLAGS)

# Directory for intermediate files and object files
ObjDir  := .obj
DepDir  := .dep

ifdef WIN32
CFLAGS += -fno-strict-aliasing
endif

# For linking.	
ifdef WIN32
LFLAGS  := -Wl,-subsystem,console
else
LFLAGS  := -m32 -lgcc -lc -lm -lrt -ldl
endif
LFLAGS += -L../hdf5/lib -lhdf5 -lhdf5_hl

# common sources
ROOT_SRC := main.c                            \

ifdef WIN32
ROOT_SRC += n2h5_res.rc
endif

# Directory of the resources (inside ROOT_DIR)
ResDir  := res/

ROOT_OBJS :=  $(patsubst %.c, $(ObjDir)/%$(DEBUG).o, $(filter %.c,$(ROOT_SRC)))   \
              $(patsubst %.rc, $(ObjDir)/%$(DEBUG).o, $(filter %.rc,$(ROOT_SRC))) \

#### This tag must be the first build tag
all: prepare $(BinDir)/$(PROGRAM)

### Make nPlay server (debug version)
.PHONY: debug
debug: DEBUG := d
debug:
	$(MAKE) all DEBUG=$(DEBUG)

# This will make the nsp program
$(BinDir)/$(PROGRAM): $(ROOT_OBJS)
	$(CC) -o $(BinDir)/$(PROGRAM) $(ROOT_OBJS) $(LFLAGS)
	
# the object files
$(ObjDir)/%$(DEBUG).o : $(ROOT_DIR)%.c Makefile
	@echo creating $@ ...
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

ifdef WIN32
$(ObjDir)/%$(DEBUG).o : $(ROOT_DIR)$(ResDir)%.rc Makefile
	@echo creating $@ ...
	windres -i $< -o $@ --include-dir=$(ROOT_DIR)$(ResDir)
endif

# For installing to system wide use
.PHONY: install
install: $(PROGRAM)
	@list='$(subdirs)'; for subdir in $$list; do test "$$subdir" = . || (cd $$subdir && $(MAKE) install); done
	cp -pf $(PWD)/$(PROGRAM) /usr/local/bin
	@chown $(shell whoami):$(shell whoami) /usr/local/bin/$(PROGRAM)
	@chmod 755 /usr/local/bin/$(PROGRAM)

.PHONY: uninstall
uninstall:
	rm -f /usr/local/bin/$(PROGRAM)
	@list='$(subdirs)'; for subdir in $$list; do test "$$subdir" = . || (cd $$subdir && $(MAKE) uninstall); done


# Clean out all files leaving installed files alone
.PHONY: clean
clean: $(subdirs)
ifdef WIN32
	@if exist $(ObjDir) del /s /q $(ObjDir)\\*.*
	@if exist $(ObjDir) rmdir $(ObjDir)
	@if exist $(BinDir) del /s /q $(BinDir)\\*.*
	@if exist $(BinDir) rmdir $(BinDir)
else
	@cd $< && $(MAKE) clean
	rm -f *.[ois] Makefile~ core* 
	rm -rf $(ObjDir)
	rm -rf $(BinDir)
endif

.PHONY: prepare
prepare:
ifdef WIN32
	@if not exist $(ObjDir)   mkdir $(ObjDir)
	@if not exist $(BinDir)   mkdir $(BinDir)
else
	@mkdir -p $(ObjDir)
	@mkdir -p $(BinDir)
endif

