ifndef PSIMAKEFILE
PSIMAKEFILE=/ioc/tools/driver.makefile
endif

INSTALLS += $(INSTALL_DOCS)
DOCS=README
INSTALL_DOCS=$(addprefix $(INSTALL_DOC)/,$(DOCS))

include $(PSIMAKEFILE)

EXCLUDE_VERSIONS = 3.13
BUILDCLASSES += Linux

HEADERS += iocshDeclWrapper.h

test: install
	$(MAKE) -C test test

clean::
	$(MAKE) -C test $@

ifdef DEBUG
ifndef T_A
  install:: pri
else
  ifdef INSTALLRULE
$(INSTALLRULE) pri
  endif
endif

.PHONY: pri

pri:
	echo INSTALLS $(INSTALLS)
	echo INSTALLDOCS $(INSTALL_DOCS)
endif

$(INSTALL_DOCS): $(addprefix ../,$(DOCS))
	echo "Installing Doc $@"
	$(MKDIR) $(@D)
	$(RM) $@
	cp $^ $@
	chmod 0444 $@
