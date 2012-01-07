default: installed all viewer

TARBALL = build/Prosilica\ GigE\ SDK\ 1.20\ Linux.tgz
TARBALL_URL = http://pr.willowgarage.com/downloads/Prosilica%20GigE%20SDK%201.20%20Linux.tgz
SOURCE_DIR = build/Prosilica_GigE_SDK
INITIAL_DIR = build/Prosilica\ GigE\ SDK
ARCH = $(shell uname -m)
ifeq ($(ARCH), i686)
ARCH_DIR = x86
else ifeq ($(ARCH), i486)
ARCH_DIR = x86
else ifeq ($(ARCH), i386)
ARCH_DIR = x86
else ifeq ($(ARCH), x86_64)
ARCH_DIR = x64
else ifeq ($(ARCH), ppc)
ARCH_DIR = ppc
endif
BIN_DIR = $(SOURCE_DIR)/bin-pc/$(ARCH_DIR)

# Spaces in tarball file name screw this up, awesome
#MD5SUM_FILE = $(TARBALL).md5sum
include $(shell rospack find mk)/download_unpack_build.mk
include $(shell rospack find mk)/cmake.mk

installed: wiped $(SOURCE_DIR)/unpacked
ifndef ARCH_DIR
	@echo "*** ERROR: Unsupported architecture $(ARCH) ***"
	@exit 2
endif
	-mkdir include
	cp $(SOURCE_DIR)/inc-pc/PvApi.h $(SOURCE_DIR)/inc-pc/PvRegIo.h include
	-mkdir lib
	cp $(BIN_DIR)/libPvAPI.so lib
	-mkdir bin
	cp $(BIN_DIR)/SampleViewer bin/SampleViewerBadRpath
	touch installed

# Create SampleViewer script that run the appropriate SampleViewer binary with libPvAPI.so in
# the library path.
viewer: all
	@echo '#!/bin/bash' > bin/SampleViewer
	@echo 'PKG_DIR=$$(rospack find prosilica_gige_sdk)' >> bin/SampleViewer
	@echo 'LIB_DIR=$${PKG_DIR}/lib' >> bin/SampleViewer
	@echo 'EXE_DIR=$${PKG_DIR}/bin' >> bin/SampleViewer
	@echo 'LD_LIBRARY_PATH=$${LIB_DIR}:$${LD_LIBRARY_PATH} $${EXE_DIR}/SampleViewerBadRpath $$@' >> bin/SampleViewer
	@chmod +x bin/SampleViewer

wiped: Makefile
	make wipe
	touch wiped

wipe: clean
	rm -rf include lib bin
	rm -f *~ installed
