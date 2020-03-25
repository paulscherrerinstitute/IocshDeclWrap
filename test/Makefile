ifndef PSIMAKEFILE
PSIMAKEFILE=/ioc/tools/driver.makefile
endif
include $(PSIMAKEFILE)

EXCLUDE_VERSIONS = 3.13
BUILDCLASSES += Linux

MODULE = iocshDeclWrapperTest

USR_INCLUDES+= -I../..

HEADERS += iocshDeclWrapper.h

debug:: pri

ifndef T_A
  install:: pri
else
  ifdef INSTALLRULE
$(INSTALLRULE) pri
  endif
endif

${DEPFILE}: hack.d

# Avoid installing to main module directory
hack.d:
	echo MODULE_LOCATION=$(dir $(USERMAKEFILE)) > $@

# Note: iocsh always terminates with a SIGTERM to itself;
#       therefore the test seems to fail even if it passes :-(
test: build
	iocsh -r iocshDeclWrapperTest < test.cmd

pri:
	echo MODULE_LOCATION $(MODULE_LOCATION)
	echo INSTALL_DBD     $(INSTALL_DBD)
	echo INSTALL_REV     $(INSTALL_REV)
