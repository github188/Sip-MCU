ACLOCAL_AMFLAGS = -I m4

plugindir = $(libdir)/mediastreamer/plugins

plugin_LTLIBRARIES = libmsilbc.la

libmsilbc_la_SOURCES = ilbc.c

libmsilbc_la_CFLAGS = $(ORTP_CFLAGS) $(MEDIASTREAMER_CFLAGS) $(ILBC_CFLAGS) \
	$(MSILBC_CFLAGS)

libmsilbc_la_LIBADD = $(ORTP_LIBS) $(MEDIASTREAMER_LIBS) $(ILBC_LIBS)

libmsilbc_la_LDFLAGS = -module  -no-undefined


INSTALLDIR=$(shell cd $(top_builddir) && pwd)/$(PACKAGE)-install
INSTALLDIR_WITH_PREFIX=$(INSTALLDIR)/$(prefix)
ZIPFILE=$(shell cd $(top_builddir) && pwd)/$(PACKAGE)-win32-$(VERSION).zip
ZIP_EXCLUDED=include 

ISS_SCRIPT=msilbc.iss
ISS_SCRIPT_PATH=$(shell cd $(top_srcdir) && pwd)/$(ISS_SCRIPT)
#path to Inno Setup 5 compiler
ISCC=/c/Program\ Files/Inno\ Setup\ 5/ISCC.exe
PACKAGE_WIN32_FILELIST=$(PACKAGE)-win32.filelist

setup.exe: zip
	cd $(INSTALLDIR_WITH_PREFIX) && \
	rm -f $(PACKAGE_WIN32_FILELIST) && \
	for file in `find` ; do \
		if ! test -d $$file ; then \
			echo "Source: $$file; Destdir: {app}\\`dirname $$file`; Flags: ignoreversion" \
			>> $(PACKAGE_WIN32_FILELIST) ;\
		fi \
	done
	cp $(ISS_SCRIPT) $(INSTALLDIR_WITH_PREFIX)/.
	cd $(INSTALLDIR_WITH_PREFIX) && \
	$(ISCC) $(ISS_SCRIPT) 
	mv $(INSTALLDIR_WITH_PREFIX)/Output/setup.exe $(PACKAGE)-$(VERSION)-setup.exe
	rm -rf $(INSTALLDIR_WITH_PREFIX)/Output
	rm -f $(INSTALLDIR_WITH_PREFIX)/$(PACKAGE_WIN32_FILELIST)
	rm -f $(INSTALLDIR_WITH_PREFIX)/$(ISS_SCRIPT)

zip:
	rm -f $(ZIPFILE)
	rm -rf $(INSTALLDIR)
	mkdir -p $(INSTALLDIR)
	make install DESTDIR=$(INSTALLDIR)
	- cp -f $(prefix)/bin/libilbc-0.dll $(INSTALLDIR)/$(plugindir)/.
	cp -f $(top_srcdir)/README $(INSTALLDIR_WITH_PREFIX)/.
	cp -f $(top_srcdir)/COPYING $(INSTALLDIR_WITH_PREFIX)/.
	cd $(INSTALLDIR)/$(prefix) && rm -rf $(ZIP_EXCLUDED) && \
	zip -r $(ZIPFILE) *

clean-local:
	-rm -rf $(PACKAGE)-install
	-rm -f $(PACKAGE)-win32-$(VERSION).zip
