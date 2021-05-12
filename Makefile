ifndef PSIMAKEFILE
PSIMAKEFILE=/ioc/tools/driver.makefile
endif

include $(PSIMAKEFILE)
include $(EPICS_MODULES)/makeUtils/latest/utils.mk

EXCLUDE_VERSIONS = 3.13
BUILDCLASSES += Linux

HEADERS += iocshDeclWrapper.h

SOURCES=-none-

test: install
	$(MAKE) -C test test

ifndef EPICSVERSION
clean:: test.clean
endif

.PHONY: clean test.clean

test.clean:
	$(MAKE) -C test $(@:test.%=%)

$(MODULE_LOCATION)/README.md: README.md
	mkdir -p $(@D)
	cp $^ $(@D)
	chmod 0444 $@

ifdef INSTALL_MODULE_TOP_RULE
$(INSTALL_MODULE_TOP_RULE) $(MODULE_LOCATION)/README.md
endif
