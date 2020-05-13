ifndef PSIMAKEFILE
PSIMAKEFILE=/ioc/tools/driver.makefile
endif
include $(PSIMAKEFILE)

EXCLUDE_VERSIONS = 3.13
BUILDCLASSES += Linux

PYTHON=python3
IOCSH=./iocshWrapper.sh

MODULE = iocshDeclWrapperTest

# Always use local version
IocshDeclWrapper_VERSION=dummyDontUse

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

ifneq ($(REAL_INSTALL),YES)
HACK_D=hack.d
else
HACK_D=rmhack
endif

${DEPFILE}: $(HACK_D)

# Avoid installing to main module directory
hack.d:
	echo MODULE_LOCATION=$(dir $(USERMAKEFILE)) > $@

rmhack:
	$(RM) hack.d

.PHONY: rmhack test.log

test.log: build

test.log:
	$(RM) $@
	echo exit | $(IOCSH) -r iocshDeclWrapperTest -c iocInit -c 'eltc 0' test.cmd > $@


# Note: iocsh always terminates with a SIGTERM to itself;
#       therefore the test seems to fail even if it passes :-(
test: test.log
	$(PYTHON) checkOutput.py < $<
	echo "TEST PASSED"

pri:
	echo MODULE_LOCATION $(MODULE_LOCATION)
	echo INSTALL_DBD     $(INSTALL_DBD)
	echo INSTALL_REV     $(INSTALL_REV)

clean::
	$(RM) test.log
