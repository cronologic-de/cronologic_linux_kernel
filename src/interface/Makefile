# -----------------------------------------------------------------------------
# 						Crono Userspace Library 
# -----------------------------------------------------------------------------

#_____________________
# Set global variables
#
LIBPATH		:= ../../lib
LIBINCPATH	:= ../../lib/include
CRONODRVPATH:= ../driver 
# INC 		:= -I${shell pwd}/$(LIBINCPATH) -I/usr/include -I${shell pwd}/crono_dma \
#			   -I${shell pwd}/$(CRONODRVPATH) -I${shell pwd}/components -I${shell pwd}/
INC 		:= -I${shell pwd}/$(LIBINCPATH) -I/usr/include -I${shell pwd}/$(CRONODRVPATH) -I${shell pwd}/ 

# Module Full Version: <Version number>.<minor number>.<release number>
LIBVER		:= .1
LIBFULLVER	:= $(LIBVER).0.1

#
# Compiler flags
#
GCC 		:= g++

# _____________________________________________________________________________
# Default build
#
all: release debug release_64 debug_64
clean: cleanrelease cleandebug cleanrelease_64 cleandebug_64

# _____________________________________________________________________________
# 32 Bit Release build settings
#
RELDIR 		:= ../../build/interface/release
RELCFLAGS 	:= -g -Wall -m32
RELTARGET	:= crono_pci_drvinterface
RELLIBNAME	:= $(RELTARGET).a
RELLDFLAGS	:= -m32
ELOBJFILES 	:= $(RELDIR)/crono_kernel_interface.o $(RELDIR)/sysfs.o

release: $(RELDIR)/$(RELLIBNAME)

$(RELDIR)/$(RELLIBNAME): $(RELDIR)/crono_kernel_interface.o $(RELDIR)/sysfs.o
# Create the static library as an archive of the `.o` files
	ar rcs $(RELDIR)/$(RELLIBNAME) $(RELOBJFILES)
# Copy release version of the library to .\lib\x86
	cp $(RELDIR)/$(RELLIBNAME) $(LIBPATH)/x86/$(RELLIBNAME)

$(RELDIR)/crono_kernel_interface.o: crono_kernel_interface.cpp $(RELDIR)/sysfs.o \
									crono_kernel_interface_internal.h $(LIBINCPATH)/crono_kernel_interface.h
# Create output build directory if not already created
	mkdir -p $(RELDIR)
# Create `.o` file
	$(call CRONO_MAKE_CPP_FILE_RULE,crono_kernel_interface,$(RELDIR),$(RELCFLAGS),$(RELLDFLAGS))

$(RELDIR)/sysfs.o: sysfs.cpp crono_kernel_interface_internal.h $(LIBINCPATH)/crono_kernel_interface.h
# Create output build directory if not already created
	mkdir -p $(RELDIR)
# Create `.o` file
	$(call CRONO_MAKE_CPP_FILE_RULE,sysfs,$(RELDIR),$(RELCFLAGS),$(RELLDFLAGS))

cleanrelease: 
	$(call CRONO_MAKE_CLEAN_OUTPUT_FILES_RULE,$(RELDIR),$(RELLIBNAME))

# _____________________________________________________________________________
# 32 Bit Debug build settings
#
DBGDIR 		:= ../../build/interface/debug
DBGCFLAGS 	:= -g -Wall -m32 -DCRONO_DEBUG_ENABLED
DBGTARGET	:= crono_pci_drvinterface
DBGLIBNAME	:= $(DBGTARGET).a
DBGLDFLAGS	:= -m32
DBGOBJFILES := $(DBGDIR)/crono_kernel_interface.o $(DBGDIR)/sysfs.o

debug: $(DBGDIR)/$(DBGLIBNAME) 

$(DBGDIR)/$(DBGLIBNAME): $(DBGDIR)/crono_kernel_interface.o $(DBGDIR)/sysfs.o 
# Create the static library as an archive of the `.o` files
	ar rcs $(DBGDIR)/$(DBGLIBNAME) $(DBGOBJFILES)

$(DBGDIR)/crono_kernel_interface.o: crono_kernel_interface.cpp $(DBGDIR)/sysfs.o \
								    crono_kernel_interface_internal.h $(LIBINCPATH)/crono_kernel_interface.h
# Create output build directory if not already created
	mkdir -p $(DBGDIR)
# Create `.o` file
	$(call CRONO_MAKE_CPP_FILE_RULE,crono_kernel_interface,$(DBGDIR),$(DBGCFLAGS),$(DBGLDFLAGS))

$(DBGDIR)/sysfs.o: sysfs.cpp crono_kernel_interface_internal.h $(LIBINCPATH)/crono_kernel_interface.h
# Create output build directory if not already created
	mkdir -p $(DBGDIR)
# Create `.o` file
	$(call CRONO_MAKE_CPP_FILE_RULE,sysfs,$(DBGDIR),$(DBGCFLAGS),$(DBGLDFLAGS))

cleandebug: 
	$(call CRONO_MAKE_CLEAN_OUTPUT_FILES_RULE,$(DBGDIR),$(DBGLIBNAME))

# _____________________________________________________________________________
# 64 Bit Release build settings
#
REL64DIR 		:= ../../build/interface/release_64
REL64CFLAGS 	:= -g -Wall -m64
REL64TARGET		:= crono_pci_drvinterface_64
REL64LIBNAME	:= $(REL64TARGET).a
REL64LDFLAGS	:= -m64
EL64OBJFILES 	:= $(REL64DIR)/crono_kernel_interface.o $(REL64DIR)/sysfs.o

release_64: $(REL64DIR)/$(REL64LIBNAME)

$(REL64DIR)/$(REL64LIBNAME): $(REL64DIR)/crono_kernel_interface.o $(REL64DIR)/sysfs.o
# Create the static library as an archive of the `.o` files
	ar rcs $(REL64DIR)/$(REL64LIBNAME) $(REL64OBJFILES)
# Copy release version of the library to .\lib\x64
	cp $(REL64DIR)/$(REL64LIBNAME) $(LIBPATH)/x64/$(REL64LIBNAME)

$(REL64DIR)/crono_kernel_interface.o: crono_kernel_interface.cpp $(REL64DIR)/sysfs.o \
									  crono_kernel_interface_internal.h $(LIBINCPATH)/crono_kernel_interface.h
# Create output build directory if not already created
	mkdir -p $(REL64DIR)
# Create `.o` file
	$(call CRONO_MAKE_CPP_FILE_RULE,crono_kernel_interface,$(REL64DIR),$(REL64CFLAGS),$(REL64LDFLAGS))

$(REL64DIR)/sysfs.o: sysfs.cpp crono_kernel_interface_internal.h $(LIBINCPATH)/crono_kernel_interface.h
# Create output build directory if not already created
	mkdir -p $(REL64DIR)
# Create `.o` file
	$(call CRONO_MAKE_CPP_FILE_RULE,sysfs,$(REL64DIR),$(REL64CFLAGS),$(REL64LDFLAGS))

cleanrelease_64: 
	$(call CRONO_MAKE_CLEAN_OUTPUT_FILES_RULE,$(REL64DIR),$(REL64LIBNAME))

# _____________________________________________________________________________
# 64 Bit Debug build settings
#
DBG64DIR 		:= ../../build/interface/debug_64
DBG64CFLAGS 	:= -g -Wall -m64 -DCRONO_DEBUG_ENABLED
DBG64TARGET		:= crono_pci_drvinterface_64
DBG64LIBNAME	:= $(DBG64TARGET).a
DBG64LDFLAGS	:= -m64
DBG64OBJFILES 	:= $(DBG64DIR)/crono_kernel_interface.o $(DBG64DIR)/sysfs.o

debug_64: $(DBG64DIR)/$(DBG64LIBNAME) 

$(DBG64DIR)/$(DBG64LIBNAME): $(DBG64DIR)/crono_kernel_interface.o $(DBG64DIR)/sysfs.o 
# Create the static library as an archive of the `.o` files
	ar rcs $(DBG64DIR)/$(DBG64LIBNAME) $(DBG64OBJFILES)

$(DBG64DIR)/crono_kernel_interface.o: crono_kernel_interface.cpp $(DBG64DIR)/sysfs.o \
									  crono_kernel_interface_internal.h $(LIBINCPATH)/crono_kernel_interface.h
# Create output build directory if not already created
	mkdir -p $(DBG64DIR)
# Create `.o` file
	$(call CRONO_MAKE_CPP_FILE_RULE,crono_kernel_interface,$(DBG64DIR),$(DBG64CFLAGS),$(DBG64LDFLAGS))

$(DBG64DIR)/sysfs.o: sysfs.cpp crono_kernel_interface_internal.h $(LIBINCPATH)/crono_kernel_interface.h
# Create output build directory if not already created
	mkdir -p $(DBG64DIR)
# Create `.o` file
	$(call CRONO_MAKE_CPP_FILE_RULE,sysfs,$(DBG64DIR),$(DBG64CFLAGS),$(DBG64LDFLAGS))

cleandebug_64: 
	$(call CRONO_MAKE_CLEAN_OUTPUT_FILES_RULE,$(DBG64DIR),$(DBG64LIBNAME))

# _____________________________________________________________________________
# General rules to avoid `Looking for an implicit rule for ...` message when using `-d` option
#
sysfs.cpp:
	# Do nothing
crono_kernel_interface.cpp: 
	# Do nothing
crono_kernel_interface_internal.h:
	# Do nothing
../../lib/include/crono_kernel_interface.h: 
	# Do nothing
Makefile: 
	# Do nothing
release: 
	# Do nothing
debug: 
	# Do nothing
release_64:
	# Do nothing
debug_64: 

# _____________________________________________________________________________
# Custom Functions
#
define CRONO_MAKE_CLEAN_FILE
	$(if $(filter-out "","$(wildcard $1)"),-rm $1,)
endef

# $1: DIR
# $2: LIBNAME
define CRONO_MAKE_CLEAN_OUTPUT_FILES_RULE
	$(call CRONO_MAKE_CLEAN_FILE,$(1)/$(2))
	$(call CRONO_MAKE_CLEAN_FILE,$(1)/*.o)
endef

# $1: File Name
# $2: DIR
# $3: CFLAGS
# $4: LDFLAGS
# Compiles the cpp file, and move .o to DIR
define CRONO_MAKE_CPP_FILE_RULE 
	$(GCC) $(INC) $(3) -c $(1).cpp $(4) -fpic
	mv $(1).o $(2)/$(1).o 
endef
