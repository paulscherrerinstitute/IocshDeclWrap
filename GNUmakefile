ifndef PSIMAKEFILE
PSIMAKEFILE=/ioc/tools/driver.makefile
endif
include $(PSIMAKEFILE)

EXCLUDE_VERSIONS = 3.13
BUILDCLASSES += Linux

HEADERS += iocshDeclWrapper.h

test: install
	$(MAKE) -C test test

clean::
	$(MAKE) -C test $@

