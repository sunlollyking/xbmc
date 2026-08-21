-include ../../Makefile.include
include POPPLER-VERSION
DEPS = Makefile POPPLER-VERSION ../../download-files.include

# Only the PDF parsing and rasterising is wanted, for rendering game manuals.
# Everything Poppler can additionally do is switched off, both to keep the
# dependency small and because each extra backend is more attack surface for
# a format that arrives from outside Kodi.
#
#   CPP        the API Kodi uses, and the only frontend built
#   UTILS      the pdftoppm family of command line tools
#   GLIB/QT    bindings for toolkits Kodi does not use
#   NSS3       digital signature checking
#   GPGME      signature checking through GnuPG
#   LIBCURL    fetching remote documents; Kodi reads through its own VFS
#   BOOST      only used by the splash backend's optional paths
#   LIBOPENJPEG JPEG2000. Kodi has no openjpeg in depends, so it is off, which
#              means a scan encoded as JPX renders its pages blank rather than
#              failing to open. See below.
CMAKE_OPTIONS=-DCMAKE_BUILD_TYPE=Release \
              -DBUILD_SHARED_LIBS=OFF \
              -DBUILD_GTK_TESTS=OFF \
              -DBUILD_QT5_TESTS=OFF \
              -DBUILD_QT6_TESTS=OFF \
              -DBUILD_CPP_TESTS=OFF \
              -DBUILD_MANUAL_TESTS=OFF \
              -DENABLE_CPP=ON \
              -DENABLE_UTILS=OFF \
              -DENABLE_GLIB=OFF \
              -DENABLE_QT5=OFF \
              -DENABLE_QT6=OFF \
              -DENABLE_NSS3=OFF \
              -DENABLE_GPGME=OFF \
              -DENABLE_LIBCURL=OFF \
              -DENABLE_BOOST=OFF \
              -DENABLE_LIBOPENJPEG=none \
              -DENABLE_LCMS=ON \
              -DWITH_Cairo=OFF \
              -DWITH_NSS3=OFF \
              -DRUN_GPERF_IF_PRESENT=OFF

ifeq ($(CROSS_COMPILING), yes)
  DEPS += ../../Makefile.include
else
  ROOT_DIR := $(shell dirname $(realpath $(lastword $(MAKEFILE_LIST))))
  ifeq ($(PLATFORM),)
    PLATFORM = native
    TARBALLS_LOCATION = $(ROOT_DIR)
    BASE_URL ?= https://poppler.freedesktop.org
    RETRIEVE_TOOL := curl
    RETRIEVE_TOOL_FLAGS := -LsS --create-dirs --retry 10 --retry-connrefused -o
    ARCHIVE_TOOL := tar
    ARCHIVE_TOOL_FLAGS := --strip-components=1 -xf
    CMAKE := cmake
    CMAKE_OPTIONS := -DCMAKE_INSTALL_PREFIX=$(PREFIX) -DCMAKE_INSTALL_LIBDIR=lib $(CMAKE_OPTIONS)
    HASH_TOOL = sha512sum
    HASH_TOOL_FLAGS = -c --status
  endif
endif

LIBDYLIB=$(PLATFORM)/$(BYPRODUCT)

include ../../download-files.include

.PHONY: .installed-native

all: .installed-$(PLATFORM)

.configured-$(PLATFORM): $(DEPS) | $(TARBALLS_LOCATION)/$(ARCHIVE).$(HASH_TYPE)
ifeq ($(PREFIX),)
	@echo
	@echo "ERROR: please set PREFIX to the kodi install path e.g. make PREFIX=/usr/local"
	@exit 1
endif
	rm -rf $(PLATFORM); mkdir -p $(PLATFORM)/build
	cd $(PLATFORM); $(ARCHIVE_TOOL) $(ARCHIVE_TOOL_FLAGS) $(TARBALLS_LOCATION)/$(ARCHIVE)
	cd $(PLATFORM)/build; $(CMAKE) $(CMAKE_OPTIONS) ..
	touch $@

.installed-$(PLATFORM): .configured-$(PLATFORM)
	$(MAKE) -C $(PLATFORM)/build install
	touch $@

clean:
	$(MAKE) -C $(PLATFORM)/build clean
	rm -f .installed-$(PLATFORM) .configured-$(PLATFORM)

distclean:
	rm -rf $(PLATFORM) .installed-$(PLATFORM) .configured-$(PLATFORM)
