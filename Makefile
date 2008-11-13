#!gmake

include make/system.mk

SUBDIRS = \
	externals \
	lib \
	server \
	tools \
	examples \
	contrib \
	proto \
	tests

.PHONY: docs

TARGETS     = precompile subdirs postcompile # docs
CLEAN_EXTRA = $(INSTALL_FILES)

include make/rules.mk

docs:
	@$(DOXYGEN) Doxyfile

lib: precompile
lib: externals
proto: lib
tests: lib server
examples: lib
contrib: lib
server: lib
tools: lib

postcompile: subdirs
	@echo "----- Compilation successful -----"
ifeq ($(findstring NDEBUG, $(DEFFLAGS)),NDEBUG)
	@echo "Release build of Equalizer with support for:"
else
	@echo "Debug build of Equalizer with support for:"
endif
ifeq ($(findstring AGL, $(WINDOW_SYSTEM)),AGL)
	@echo "    AGL/Carbon windowing"
endif
ifeq ($(findstring GLX, $(WINDOW_SYSTEM)),GLX)
	@echo "    glX/X11 windowing"
endif
ifeq ($(findstring WGL, $(WINDOW_SYSTEM)),WGL)
	@echo "    WGL/Win32 windowing"
endif
ifeq ($(findstring EQ_USE_OPENMP, $(DEFFLAGS)),EQ_USE_OPENMP)
	@echo "    OpenMP (http://www.openmp.org/)"
endif
ifeq ($(findstring EQ_USE_PARACOMP, $(DEFFLAGS)),EQ_USE_PARACOMP)
	@echo "    Paracomp (http://paracomp.sourceforge.net/)"
endif
ifeq (Darwin,$(ARCH))
	@echo "Set DYLD_LIBRARY_PATH to $(PWD)/$(LIBRARY_DIR)"
else
	@echo "Set LD_LIBRARY_PATH to $(PWD)/$(LIBRARY_DIR)"
endif

RELNOTES: ../../trunk/website/build/documents/RelNotes/RelNotes_0.6-rc1.html
	links -dump $< > $@
