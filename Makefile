# eaglercraft-ps4 -- OpenOrbis homebrew Makefile
#
# Mirrors the structure of OpenOrbis-PS4-Toolchain/samples/*/Makefile
# (verified-working pattern), adapted for a standalone repo (fixed "source"
# dir instead of the samples convention of PROJDIR==basename(CURDIR), and an
# explicit include path to the toolchain's samples/_common dir for log.h).
#
# Requires: OO_PS4_TOOLCHAIN env var pointing at a full OpenOrbis install
# (headers + stub libs + bin/{linux,macos}/{create-fself,create-gp4,PkgTool.Core}).
# This project has NOT been compiled against that toolchain yet -- see
# README "Known risk areas" before reporting build errors as surprising.

# Package metadata.
TITLE       := EaglercraftX 1.8 (PS4)
VERSION     := 1.00
TITLE_ID    := BREW00200
CONTENT_ID  := IV0000-BREW00200_00-EAGLERCRAFTPS400

# Libraries linked into the ELF. Confirmed against the real toolchain's
# lib/*.so. -lSceVideoOut is needed for the minimal Scene2D surface in
# main.cpp (see its header comment for why a video-out handle is kept
# open at all -- no EGL/Piglet GL context needed, just a plain framebuffer).
# -lSceFreeType backs Scene2D's text drawing (GRAPHICS_USES_FONT below),
# used to show exact error codes on screen for debugging -- far more
# reliable than a hardware tester trying to decode color bars from a
# photo, which is what this replaced.
LIBS        := -lc -lkernel -lc++ -lSceSysmodule -lSceCommonDialog -lSceWebBrowserDialog -lSceVideoOut -lSceFreeType -lSceUserService

# Additional compile flags. GRAPHICS_USES_FONT enables Scene2D's
# FreeType-backed DrawText (see samples/keyboard's own Makefile for the
# same flag, verified pattern). NOTE: Make doesn't notice when a flag
# like this one changes, only when source files do -- `make clean` first
# if you flip this and see undefined-symbol link errors for
# Scene2D::InitFont/DrawText.
EXTRAFLAGS  := -DGRAPHICS_USES_FONT

# Project source/asset layout (fixed names, unlike the sample convention of
# PROJDIR == basename(CURDIR)).
PROJDIR     := source
# NOTE: $(wildcard assets/**/*) (the pattern every OpenOrbis sample Makefile
# uses) does NOT recurse in GNU Make -- ** has no special meaning to
# $(wildcard), so it silently matched nothing here and eaglercraft.html
# was missing from the built pkg. Verified wrong, fixed with `find`.
ASSETS      := $(shell find assets -type f 2>/dev/null)
LIBMODULES  := $(wildcard sce_module/*)

# Root vars
TOOLCHAIN   := $(OO_PS4_TOOLCHAIN)
COMMONDIR   := $(TOOLCHAIN)/samples/_common
INTDIR      := x64/Debug

# Define objects to build. graphics.cpp is the toolchain's own Scene2D
# helper (samples/_common/graphics.cpp) -- pulled in directly rather than
# via the sample convention's COMMONFILES wildcard, since that wildcard
# would also pull in png.cpp/ootk, which this project doesn't use or link
# against.
CFILES      := $(wildcard $(PROJDIR)/*.c)
CPPFILES    := $(wildcard $(PROJDIR)/*.cpp)
COMMONFILES := $(COMMONDIR)/graphics.cpp
OBJS        := $(patsubst $(PROJDIR)/%.c, $(INTDIR)/%.o, $(CFILES)) $(patsubst $(PROJDIR)/%.cpp, $(INTDIR)/%.o, $(CPPFILES)) $(patsubst $(COMMONDIR)/%.cpp, $(INTDIR)/%.o, $(COMMONFILES))

# Define final C/C++ flags
CFLAGS      := --target=x86_64-pc-freebsd12-elf -fPIC -funwind-tables -c $(EXTRAFLAGS) -isysroot $(TOOLCHAIN) -isystem $(TOOLCHAIN)/include -I$(COMMONDIR)
CXXFLAGS    := $(CFLAGS) -isystem $(TOOLCHAIN)/include/c++/v1
LDFLAGS     := -m elf_x86_64 -pie --script $(TOOLCHAIN)/link.x --eh-frame-hdr -L$(TOOLCHAIN)/lib $(LIBS) $(TOOLCHAIN)/lib/crt1.o

# Create the intermediate directory incase it doesn't already exist.
_unused     := $(shell mkdir -p $(INTDIR))

# Check for linux vs macOS and account for clang/ld path
UNAME_S     := $(shell uname -s)

ifeq ($(UNAME_S),Linux)
		CC      := clang
		CCX     := clang++
		LD      := ld.lld
		CDIR    := linux
endif
ifeq ($(UNAME_S),Darwin)
		CC      := /usr/local/opt/llvm/bin/clang
		CCX     := /usr/local/opt/llvm/bin/clang++
		LD      := /usr/local/opt/llvm/bin/ld.lld
		CDIR    := macos
endif

all: $(CONTENT_ID).pkg

$(CONTENT_ID).pkg: pkg.gp4
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core pkg_build $< .

pkg.gp4: eboot.bin sce_sys/about/right.sprx sce_sys/param.sfo sce_sys/icon0.png $(LIBMODULES) $(ASSETS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-gp4 -out $@ --content-id=$(CONTENT_ID) --files "$^"

sce_sys/param.sfo: Makefile
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_new $@
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_TYPE --type Integer --maxsize 4 --value 1
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ APP_VER --type Utf8 --maxsize 8 --value '$(VERSION)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ ATTRIBUTE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CATEGORY --type Utf8 --maxsize 4 --value 'gd'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ CONTENT_ID --type Utf8 --maxsize 48 --value '$(CONTENT_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ DOWNLOAD_DATA_SIZE --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ SYSTEM_VER --type Integer --maxsize 4 --value 0
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE --type Utf8 --maxsize 128 --value '$(TITLE)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ TITLE_ID --type Utf8 --maxsize 12 --value '$(TITLE_ID)'
	$(TOOLCHAIN)/bin/$(CDIR)/PkgTool.Core sfo_setentry $@ VERSION --type Utf8 --maxsize 8 --value '$(VERSION)'

eboot.bin: $(INTDIR) $(OBJS)
	$(LD) $(INTDIR)/*.o -o $(INTDIR)/eaglercraft-ps4.elf $(LDFLAGS)
	$(TOOLCHAIN)/bin/$(CDIR)/create-fself -in=$(INTDIR)/eaglercraft-ps4.elf -out=$(INTDIR)/eaglercraft-ps4.oelf --eboot "eboot.bin" --paid 0x3800000000000011

$(INTDIR)/%.o: $(PROJDIR)/%.c
	$(CC) $(CFLAGS) -o $@ $<

$(INTDIR)/%.o: $(PROJDIR)/%.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

$(INTDIR)/%.o: $(COMMONDIR)/%.cpp
	$(CCX) $(CXXFLAGS) -o $@ $<

clean:
	rm -f $(CONTENT_ID).pkg pkg.gp4 sce_sys/param.sfo eboot.bin \
		$(INTDIR)/eaglercraft-ps4.elf $(INTDIR)/eaglercraft-ps4.oelf $(OBJS)
