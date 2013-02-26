######################################################################################
##
## Makefile for Cerebus API
## (c) Copyright 2012 - 2013 Blackrock Microsystems
##
##           Workfile: Makefile 
##           Archive: /Cerebus/Human/LinuxApps/cbmex/Makefile
##           Revision: 1 
##           Date: 4/29/12 12:04p 
##           Author: Ehsan
##
##
## Major targets:
## all          - make all the following targets:
## sdk          - make libcbsdk
## mex          - make cbmex
## pysdk        - make python binding for sdk
##
## Special targets:
## x64          - make all targets in 64bit
## debug        - make all targets in debug mode
## install      - place all scripts and binaries in the conventional directories
## uninstall    - remove all scripts and binaries that may be in OS directory tree
## clean        - clean up
## test         - make sdk, install it and make the test
##
## use ARCH=x64 (ex: make all ARCH=x64) in order to make 64-bit for any target
## use DEBUG=d (ex: make mex DEBUG=d) in order to force debug information for any target
##
######################################################################################

#==========================================================================
# Operating System
#==========================================================================
OS := $(shell echo %OS%)
ifeq ($(OS),Windows_NT)
WIN32 := 1
PWD := $(CURDIR)
else
# figure out OSX or Linux
endif

ifeq ($(ARCH),x64)
ARCHFLAG := -m64
else
ARCH :=
ARCHFLAG := -m32
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

CC      := gcc
CXX     := g++
MOC     := moc-qt4
PYTHONVER  := python2.7

# SDK naming
CBSDKLIBNAME  = cbsdk$(ARCH)$(DEBUG)
CBSDKBASENAME = lib$(CBSDKLIBNAME)
CBSDKSO       = $(CBSDKBASENAME).so
CBSDKVER      = $(shell $(ObjDir)/$(CBSDKMAINBIN))
# internal soname
CBSDKSONAME   = $(CBSDKBASENAME).so.$(CBSDKVER)
# SDK versiong binary
CBSDKMAINBIN      = cbsdk$(ARCH)$(DEBUG)
# SDK versiong binary
CBSDKTESTBIN      = testcbsdk$(ARCH)$(DEBUG)

# Python naming
CBPYBASENAME  = cbpy$(ARCH)
CBPYSO        = $(CBPYBASENAME)$(DEBUG).so

# Mex naming
ifdef OSX
CBMEXSO       = cbmex$(DEBUG).mexmaci64
CBMEXLIBDIRS := $(PWD)/../Matlab/lib/osx64
BINPREFIX    := ../osx64
else
ifeq ($(ARCH),x64)
CBMEXSO       = cbmex$(DEBUG).mexa64
CBMEXLIBDIRS := -L$(PWD)/../Matlab/lib/linux64
BINPREFIX    := ../linux64
else
CBMEXSO       = cbmex$(DEBUG).mexglx
CBMEXLIBDIRS := -L$(PWD)/../Matlab/lib/linux32 
BINPREFIX    := ../linux32
endif
endif

INCLUDEDIRS := \
	$(shell pkg-config --cflags-only-I QtCore QtXml)\
	-I$(PWD)/../Central -I$(PWD)/../CentralCommon -I$(PWD)/../cbhwlib    \
	-I$(PWD)/../Matlab/include -I/usr/include/$(PYTHONVER) -I/usr/include/numpy \

# Additional libraries
LIBS      := -lQtCore -lQtXml -lpthread -lrt
MEXLIBS   := -lmex -lmx
PYSDKLIBS := -l$(PYTHONVER)

CFLAGS := -Wall -Wno-reorder $(ARCHFLAG) -fPIC -fvisibility=hidden   \
	-maccumulate-outgoing-args   \
	-funroll-loops -finline-functions                         \
	-fno-strict-aliasing \
	$(INCLUDEDIRS)

ifneq ($(ARCH),x64)
# malign-double makes no sense in 64-bit
CFLAGS += -malign-double
endif

EXTRA_DEFINES := -DCBSDK_EXPORTS -DQT_CORE -DQT_XML -DNO_AFX -DQT_APP

CFLAGS += $(EXTRA_DEFINES)

ifdef DEBUG
BinDir  := $(BINPREFIX)/debug
CFLAGS += -O0 -g3 -UNDEBUG -DDEBUG
else
BinDir  := $(BINPREFIX)/release
CFLAGS += -O2 -DNDEBUG -UDEBUG
endif

# Directory for intermediate files and object files
ObjDir   := .obj
MocDir   := .moc$(DEBUG)
PySdkDir := .pysdk

# For linking
LDFLAGS = $(LIBDIRS) $(LIBS)
LFLAGS  = $(ARCHFLAG) -shared $(LDFLAGS) -Wl,-soname,$(CBSDKSONAME) -Wl,--no-undefined 
MEXLFLAGS = $(LFLAGS) $(MEXLIBS)
PYSDKLFLAGS = $(LFLAGS) $(PYSDKLIBS)

# common sources
COMMON_SRC := ./cbsdk.cpp                     \
              ../cbhwlib/cbhwlib.cpp          \
              ../cbhwlib/cbHwlibHi.cpp        \
              ../Central/Instrument.cpp       \
              ../Central/UDPsocket.cpp        \
              ../cbhwlib/InstNetwork.cpp      \
              ../cbhwlib/CCFUtils.cpp         \
              ../cbhwlib/CCFUtilsBinary.cpp   \
              ../cbhwlib/CCFUtilsXml.cpp      \
              ../cbhwlib/CCFUtilsXmlItems.cpp \
              ../cbhwlib/CCFUtilsConcurrent.cpp \
              ../cbhwlib/XmlFile.cpp          \

# moc'ed sources
MOC_HEADER := ../cbhwlib/InstNetwork.h        \

# Mex sources
MEX_SRC := ./cbmex.cpp                       \

# Python sources
PY_SRC := ./cbpy.cpp                       \
          ./cbpytypes.cpp                  \

# where to look for the sources
VPATH := ../cbhwlib:../Central

# object files from sources
MOC_OBJS    := $(patsubst %.h, $(ObjDir)/$(MocDir)/moc_%$(ARCH)$(DEBUG).o, $(notdir $(MOC_HEADER)))
COMMON_OBJS := $(MOC_OBJS) $(patsubst %.cpp, $(ObjDir)/%$(ARCH)$(DEBUG).o, $(notdir $(COMMON_SRC)))
MEX_OBJS    := $(patsubst %.cpp, $(ObjDir)/%$(ARCH)$(DEBUG).o, $(notdir $(MEX_SRC)))
PY_OBJS     := $(patsubst %.cpp, $(ObjDir)/$(PySdkDir)/%$(ARCH)$(DEBUG).o, $(notdir $(PY_SRC)))

#### This tag must be the first build tag
all: sdk mex pysdk

sdk: prepare $(BinDir)/$(CBSDKSO)
	@echo shared library done. 
	
mex: LIBDIRS := $(CBMEXLIBDIRS) 
mex: prepare $(BinDir)/$(CBMEXSO)
	@echo Matlab extension done. 

test: sdk install $(BinDir)/$(CBSDKTESTBIN)
	@echo Test suite done.

pysdk: prepare $(BinDir)/$(CBPYSO)
	@echo Python extension done. 

.PHONY: debug
debug: DEBUG := d
debug:
	$(MAKE) all DEBUG=$(DEBUG)

.PHONY: x64
x64: ARCH := x64
x64: all
	$(MAKE) all ARCH=$(ARCH)

# the "common" object files
$(ObjDir)/%$(ARCH)$(DEBUG).o : %.cpp Makefile
	@echo creating $@ ...
	$(CXX) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<
	
# the "pysdk" object files
$(ObjDir)/$(PySdkDir)/%$(ARCH)$(DEBUG).o : EXTRA_CFLAGS += -DCBPYSDK
$(ObjDir)/$(PySdkDir)/%$(ARCH)$(DEBUG).o : %.cpp Makefile
	@echo creating $@ ...
	$(CXX) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

# the SDK version
$(ObjDir)/$(CBSDKMAINBIN) : ./main.cpp ../CentralCommon/BmiVersion.h Makefile
	@echo creating $@ ...
	$(CXX) $(CFLAGS) $(EXTRA_CFLAGS) -o $@ $<

# "moc" cpp files
.SECONDARY: $(patsubst %.h, $(MocDir)/moc_%.cpp, $(notdir $(MOC_HEADER)))
$(MocDir)/moc_%.cpp: ../cbhwlib/%.h Makefile 
	@echo Moc\'ing $<
	$(MOC) $(EXTRA_DEFINES) $(INCLUDEDIRS) -I"$(PWD)/$(MocDir)" $< -o $(patsubst %.h, $(MocDir)/moc_%.cpp, $(notdir $<))
$(MocDir)/moc_%.cpp: %.h Makefile 
	@echo Moc\'ing $<
	$(MOC) $(EXTRA_DEFINES) $(INCLUDEDIRS) -I"$(PWD)/$(MocDir)" $< -o $(patsubst %.h, $(MocDir)/moc_%.cpp, $(notdir $<))

# "moc" object files
$(ObjDir)/$(MocDir)/%$(ARCH)$(DEBUG).o: $(MocDir)/%.cpp Makefile
	@echo creating $@ ...
	$(CXX) $(CFLAGS) $(EXTRA_CFLAGS) -c -o $@ $<

# This will make the cbsdk shared library
$(BinDir)/$(CBSDKSO): $(ObjDir)/$(CBSDKMAINBIN) $(COMMON_OBJS)
	@echo building shared library ...
	$(CXX) -o $(BinDir)/$(CBSDKSONAME) $(COMMON_OBJS) $(LFLAGS)

# This will make the cbpy shared library
$(BinDir)/$(CBPYSO) : $(ObjDir)/$(CBSDKMAINBIN) $(PY_OBJS) $(COMMON_OBJS)
	@echo building Python extension ...
	$(CXX) -o $(BinDir)/$(CBPYSO) $(PY_OBJS) $(COMMON_OBJS) $(PYSDKLFLAGS)

# This will make the cbmex shared library
$(BinDir)/$(CBMEXSO): $(ObjDir)/$(CBSDKMAINBIN) $(MEX_OBJS) $(COMMON_OBJS)
	@echo building Matlab extension ...
	$(CXX) -o $(BinDir)/$(CBMEXSO) $(MEX_OBJS) $(COMMON_OBJS) $(MEXLFLAGS)

# the SDK test suite
$(BinDir)/$(CBSDKTESTBIN) : ./testcbsdk.cpp Makefile
	@echo creating $@ ...
	$(CXX) -I$(PWD)/../cbhwlib -L$(BinDir) -l$(CBSDKLIBNAME) -o $@ $<

# For installing to system wide use
.PHONY: install
install: $(BinDir)/$(CBSDKSONAME)
	cp -pf $(BinDir)/$(CBSDKSONAME) /usr/local/lib
	@chown $(shell whoami):$(shell whoami) /usr/local/lib/$(CBSDKSONAME)
	@chmod 755 /usr/local/lib/$(CBSDKSONAME)
	ln -sf /usr/local/lib/$(CBSDKSONAME) /usr/local/lib/$(CBSDKSO)

.PHONY: uninstall
uninstall:
	rm -f /usr/local/lib/$(CBSDKSO)
	rm -f /usr/local/lib/$(CBSDKSONAME)


# Clean out all files leaving installed files alone
.PHONY: clean
clean:
ifdef WIN32
	@if exist $(MocDir) del /s /q $(MocDir)\*
	@if exist $(MocDir) rmdir $(MocDir)
	@if exist $(ObjDir)\\$(MocDir) del /s /q $(ObjDir)\\$(MocDir)\\*.*
	@if exist $(ObjDir)\\$(MocDir) rmdir $(ObjDir)\\$(MocDir)
	@if exist $(ObjDir)\\$(PySdkDir) del /s /q $(ObjDir)\\$(PySdkDir)\\*.*
	@if exist $(ObjDir)\\$(PySdkDir) rmdir $(ObjDir)\\$(PySdkDir)
	@if exist $(ObjDir) del /s /q $(ObjDir)\\*.*
	@if exist $(ObjDir) rmdir $(ObjDir)
	@if exist $(BinDir) del /s /q $(BinDir)\\*.*
	@if exist $(BinDir) rmdir $(BinDir)
else
	rm -rf $(MocDir)
	rm -rf $(ObjDir)
	rm -rf $(BinDir)
endif

.PHONEY: prepare
prepare: prepare_dir $(ObjDir)/$(CBSDKMAINBIN)
	@echo
	@echo building started for $(CBSDKVER).$(if $(ARCH),$(ARCH),i686) ...
	@echo

.PHONY: prepare_dir
prepare_dir:
ifdef WIN32
	@if not exist $(ObjDir)   mkdir $(ObjDir)
	@if not exist $(ObjDir)\\$(MocDir)    mkdir $(ObjDir)\\$(MocDir) 
	@if not exist $(ObjDir)\\$(PySdkDir)    mkdir $(ObjDir)\\$(PySdkDir) 
	@if not exist $(MocDir)   mkdir $(MocDir)
	@if not exist $(BinDir)   mkdir $(BinDir)
else
	@mkdir -p $(ObjDir)/$(PySdkDir)
	@mkdir -p $(ObjDir)/$(MocDir)
	@mkdir -p $(MocDir)
	@mkdir -p $(BinDir)
endif
