#
# Makefile for a Video Disk Recorder plugin
#
# $Id$

# The official name of this plugin.
# This name will be used in the '-P...' option of VDR to load the plugin.
# By default the main source file also carries this name.

PLUGIN = mpv

### Configuration (edit this for your needs)
#CONFIG := -DDEBUG			# uncomment to build DEBUG

# WARNING !!!
# If you want use keyboard instead RCU, you MUST compile mpv plugin after softhd* plugin,
# or for the libvdr-softhd* library to be present on the system.
# You may compile mpv plugin in the second pass, or create symbolic link "zmpv" to vdr-plugin-mpv directory.

#USESOFTHDDEVICE = 1
#USESOFTHDCUVID = 1
#USESOFTHDVAAPI = 1
#USEVAAPIDEVICE = 1
# support refresh rate switching
XRANDR ?= $(shell pkg-config --exists xrandr && echo 1)
LIBMPV ?= $(shell pkg-config --exists mpv && echo 1)
LIBCDIO ?= $(shell pkg-config --exists libcdio && echo 1)
LIBMOUNT ?= $(shell pkg-config --exists mount && echo 1)

ifeq ($(LIBMPV),1)
CONFIG += -DUSE_LIBMPV
LIBS += $(shell pkg-config --libs mpv)
endif

ifeq ($(LIBCDIO),1)
ifeq ($(LIBMOUNT),1)
CONFIG += -DUSE_DISC
LIBS += $(shell pkg-config --libs libcdio)
LIBS += $(shell pkg-config --libs mount)
endif
endif

ifeq ($(XRANDR),1)
CONFIG += -DUSE_XRANDR
LIBS += $(shell pkg-config --libs xrandr)
endif

LIBS += $(shell pkg-config --libs x11-xcb)
### The version number of this plugin (taken from the main source file):

VERSION = $(shell grep 'static const char \*VERSION *=' $(PLUGIN).c | awk '{ print $$6 }' | sed -e 's/[";]//g')
GIT_REV = $(shell git describe --always 2>/dev/null)

### The directory environment:

# Use package data if installed...otherwise assume we're under the VDR source directory:
PKGCFG = $(if $(VDRDIR),$(shell pkg-config --variable=$(1) $(VDRDIR)/vdr.pc),$(shell PKG_CONFIG_PATH="$$PKG_CONFIG_PATH:../../.." pkg-config --variable=$(1) vdr))
LIBDIR = $(call PKGCFG,libdir)
LOCDIR = $(call PKGCFG,locdir)
PLGCFG = $(call PKGCFG,plgcfg)
PLGRESDIR = $(call PKGCFG,resdir)/plugins/$(PLUGIN)
#
TMPDIR ?= /tmp

### The compiler options:

export CFLAGS   = $(call PKGCFG,cflags)
export CXXFLAGS = $(call PKGCFG,cxxflags)
export CXXFLAGS += -Wno-switch

### The version number of VDR's plugin API:

APIVERSION = $(call PKGCFG,apiversion)

### Allow user defined options to overwrite defaults:

-include $(PLGCFG)

### The name of the distribution archive:

ARCHIVE = $(PLUGIN)-$(VERSION)
PACKAGE = vdr-$(ARCHIVE)

### The name of the shared object file:

SOFILE = libvdr-$(PLUGIN).so

### Extended softhd* libs for keyboard control

# softhddevice
ifeq ($(USESOFTHDDEVICE),1)
SHDLIB ?= $(shell pkg-config --exists libvdr-softhddevice && echo 1)
ifeq ($(SHDLIB),1)
LIBS += $(shell pkg-config --libs libvdr-softhddevice)
CONFIG += -DUSE_KEYPRESS
endif
ifneq ($(SHDLIB),0)
SHDLIB = $(DESTDIR)$(LIBDIR)/libvdr-softhddevice.so.$(APIVERSION)
ifeq ($(shell test -f $(SHDLIB); echo $$?),0)
LIBS += $(SHDLIB)
CONFIG += -DUSE_KEYPRESS
endif
endif
endif
# softhdcuvid
ifeq ($(USESOFTHDCUVID),1)
SHCLIB ?= $(shell pkg-config --exists libvdr-softhdcuvid && echo 1)
ifeq ($(SHCLIB),1)
LIBS += $(shell pkg-config --libs libvdr-softhdcuvid)
CONFIG += -DUSE_KEYPRESS
endif
ifneq ($(SHCLIB),0)
SHCLIB = $(DESTDIR)$(LIBDIR)/libvdr-softhdcuvid.so.$(APIVERSION)
ifeq ($(shell test -f $(SHCLIB); echo $$?),0)
LIBS += $(SHCLIB)
CONFIG += -DUSE_KEYPRESS
endif
endif
endif
# softhdvaapi
ifeq ($(USESOFTHDVAAPI),1)
SHVLIB ?= $(shell pkg-config --exists libvdr-softhdvaapi && echo 1)
ifeq ($(SHVLIB),1)
LIBS += $(shell pkg-config --libs libvdr-softhdvaapi)
CONFIG += -DUSE_KEYPRESS
endif
ifneq ($(SHVLIB),0)
SHVLIB = $(DESTDIR)$(LIBDIR)/libvdr-softhdvaapi.so.$(APIVERSION)
ifeq ($(shell test -f $(SHVLIB); echo $$?),0)
LIBS += $(SHVLIB)
CONFIG += -DUSE_KEYPRESS
endif
endif
endif
# vaapidevice
ifeq ($(USEVAAPIDEVICE),1)
VDLIB ?= $(shell pkg-config --exists libvdr-vaapidevice && echo 1)
ifeq ($(VDLIB),1)
LIBS += $(shell pkg-config --libs libvdr-vaapidevice)
CONFIG += -DUSE_KEYPRESS
endif
ifneq ($(VDLIB),0)
VDLIB = $(DESTDIR)$(LIBDIR)/libvdr-vaapidevice.so.$(APIVERSION)
ifeq ($(shell test -f $(VDLIB); echo $$?),0)
LIBS += $(VDLIB)
CONFIG += -DUSE_KEYPRESS
endif
endif
endif

### Includes and Defines (add further entries here):

INCLUDES +=

DEFINES += -DPLUGIN_NAME_I18N='"$(PLUGIN)"' -D_GNU_SOURCE $(CONFIG) \
	$(if $(GIT_REV), -DGIT_REV='"$(GIT_REV)"')

DEFINES += -DPLGRESDIR='"$(PLGRESDIR)"'
### The object files (add further files here):

OBJS = $(PLUGIN).o config.o control.o filebrowser.o menu_options.o osd.o player.o setup.o status.o

### The main target:

all: $(SOFILE) i18n

### Implicit rules:

%.o: %.c
	@echo CC $@
	$(Q)$(CXX) $(CXXFLAGS) -c $(DEFINES) $(INCLUDES) -o $@ $<

### Dependencies:

MAKEDEP = $(CXX) -MM -MG
DEPFILE = .dependencies
$(DEPFILE): Makefile
	@$(MAKEDEP) $(CXXFLAGS) $(DEFINES) $(INCLUDES) $(OBJS:%.o=%.c) > $@

-include $(DEPFILE)

### Internationalization (I18N):

PODIR     = po
I18Npo    = $(wildcard $(PODIR)/*.po)
I18Nmo    = $(addsuffix .mo, $(foreach file, $(I18Npo), $(basename $(file))))
I18Nmsgs  = $(addprefix $(DESTDIR)$(LOCDIR)/, $(addsuffix /LC_MESSAGES/vdr-$(PLUGIN).mo, $(notdir $(foreach file, $(I18Npo), $(basename $(file))))))
I18Npot   = $(PODIR)/$(PLUGIN).pot

%.mo: %.po
	@echo MO $@
	$(Q)msgfmt -c -o $@ $<

$(I18Npot): $(wildcard *.c)
	@echo GT $@
	$(Q)xgettext -C -cTRANSLATORS --no-wrap --no-location -k -ktr -ktrNOOP --package-name=vdr-$(PLUGIN) --package-version=$(VERSION) --msgid-bugs-address='<see README>' -o $@ `ls $^`

%.po: $(I18Npot)
	@echo PO $@
	$(Q)msgmerge -U --no-wrap --no-location --backup=none -q -N $@ $<
	@touch $@

$(I18Nmsgs): $(DESTDIR)$(LOCDIR)/%/LC_MESSAGES/vdr-$(PLUGIN).mo: $(PODIR)/%.mo
	install -D -m644 $< $@

.PHONY: i18n
i18n: $(I18Nmo) $(I18Npot)

install-i18n: $(I18Nmsgs)

### Targets:

$(SOFILE): $(OBJS)
	@echo LD $@
	$(Q)$(CXX) $(CXXFLAGS) $(LDFLAGS) -shared $(OBJS) $(LIBS) -o $@

install-lib: $(SOFILE)
	install -D $^ $(DESTDIR)$(LIBDIR)/$^.$(APIVERSION)

install: install-lib install-i18n

dist: $(I18Npo) clean
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@mkdir $(TMPDIR)/$(ARCHIVE)
	@cp -a * $(TMPDIR)/$(ARCHIVE)
	@tar cjf $(PACKAGE).tar.bz2 -C $(TMPDIR) $(ARCHIVE)
	@-rm -rf $(TMPDIR)/$(ARCHIVE)
	@echo Distribution package created as $(PACKAGE).tar.bz2

clean:
	@-rm -f $(PODIR)/*.mo $(PODIR)/*.pot
	@-rm -f $(OBJS) $(DEPFILE) *.so *.tar.bz2 core* *~
