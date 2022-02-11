KNOCONFIG         = knoconfig
KNOBUILD          = knobuild

prefix		::= $(shell ${KNOCONFIG} prefix)
libsuffix	::= $(shell ${KNOCONFIG} libsuffix)
KNO_CFLAGS	::= -I. -fPIC -Wno-multichar $(shell ${KNOCONFIG} cflags)
KNO_LIBFLAGS	::= -fPIC $(shell ${KNOCONFIG} ldflags)
KNO_LIBS	::= $(shell ${KNOCONFIG} libs)
DATADIR		::= $(DESTDIR)$(shell ${KNOCONFIG} data)
CMODULES	::= $(DESTDIR)$(shell ${KNOCONFIG} cmodules)
LIBS		::= $(shell ${KNOCONFIG} libs)
LIB		::= $(shell ${KNOCONFIG} lib)
INCLUDE		::= $(shell ${KNOCONFIG} include)
KNO_VERSION	::= $(shell ${KNOCONFIG} version)
KNO_MAJOR	::= $(shell ${KNOCONFIG} major)
KNO_MINOR	::= $(shell ${KNOCONFIG} minor)
PKG_VERSION     ::= $(shell u8_gitversion ./etc/knomod_version)
PKG_MAJOR       ::= $(shell cat ./etc/knomod_version | cut -d. -f1)
FULL_VERSION    ::= ${KNO_MAJOR}.${KNO_MINOR}.${PKG_VERSION}
PATCHLEVEL      ::= $(shell u8_gitpatchcount ./etc/knomod_version)
PATCH_VERSION   ::= ${FULL_VERSION}-${PATCHLEVEL}

PKG_NAME	::= hunspell
DPKG_NAME	::= ${PKG_NAME}_${PATCH_VERSION}

SUDO            ::= $(shell which sudo)

MKSO		  = $(CC) -shared $(LDFLAGS) $(LIBS)
SYSINSTALL        = /usr/bin/install -c
MSG		  = echo
MACLIBTOOL	  = $(CC) -dynamiclib -single_module -undefined dynamic_lookup \
			$(LDFLAGS)

GPGID           ::= ${OVERRIDE_GPGID:-FE1BC737F9F323D732AA26330620266BE5AFF294}
CODENAME	::= $(shell ${KNOCONFIG} codename)
REL_BRANCH	::= $(shell ${KNOBUILD} getbuildopt REL_BRANCH current)
REL_STATUS	::= $(shell ${KNOBUILD} getbuildopt REL_STATUS stable)
REL_PRIORITY	::= $(shell ${KNOBUILD} getbuildopt REL_PRIORITY medium)
ARCH            ::= $(shell ${KNOBUILD} getbuildopt BUILD_ARCH || uname -m)
APKREPO         ::= $(shell ${KNOBUILD} getbuildopt APKREPO /srv/repo/kno/apk)
APK_ARCH_DIR      = ${APKREPO}/staging/${ARCH}
RPMDIR		  = dist

INIT_CFLAGS        ::= ${CFLAGS}
INIT_LIBFLAGS      ::= ${LDFLAGS}
HUNSPELL_CFLAGS    ::= $(shell pkg-config --cflags hunspell 2> /dev/null || echo) 
HUNSPELL_LIBFLAGS  ::= $(shell pkg-config --libs hunspell 2> /dev/null || echo) 
XCFLAGS	  	     = ${INIT_CFLAGS} $(KNO_CFLAGS} ${HUNSPELL_CFLAGS}
XLIBFLAGS	     = ${INIT_LIBFLAGS} $(KNO_LIBFLAGS} ${HUNSPELL_LIBFLAGS}

%.o: %.c
	@$(CC) $(CFLAGS) -D_FILEINFO="\"$(shell u8_fileinfo ./$< $(dirname $(pwd))/)\"" -o $@ -c $<
	@$(MSG) CC $@ $<

default build: ${PKG_NAME}.${libsuffix}

hunspell.so: hunspell.c makefile
	@$(MKSO) $(XCFLAGS) -o $@ hunspell.c -lhunspell
	@$(MSG) MKSO  $@ $< ${XLIBFLAGS}
	@ln -sf $(@F) $(@D)/$(@F).${KNO_MAJOR}
hunspell.dylib: hunspell.c
	@$(MACLIBTOOL) -install_name \
		`basename $(@F) .dylib`.${KNO_MAJOR}.dylib \
		${CFLAGS} -o $@ $(DYLIB_FLAGS) ${XLIBFLAGS} \
		hunspell.c
	@$(MSG) MACLIBTOOL  $@ $<

TAGS: hunspell.c
	etags -o TAGS hunspell.c

${CMODULES} ${DATADIR}:
	install -d $@

install: build ${CMODULES} ${DATADIR}
	${SUDO} u8_install_shared ${PKG_NAME}.${libsuffix} ${CMODULES} ${FULL_VERSION} "${SYSINSTALL}"
	@${SUDO} ${SYSINSTALL} data/*.dic ${DATADIR}
	@echo === Installed ${DATADIR}/hyph_en_US.dic

clean:
	rm -f *.o ${PKG_NAME}/*.o *.${libsuffix}
fresh:
	make clean
	make default

gitup gitup-trunk:
	git checkout trunk && git pull

