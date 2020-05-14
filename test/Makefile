ifndef PSIMAKEFILE
PSIMAKEFILE=/ioc/tools/driver.makefile
endif


include $(PSIMAKEFILE)

MODULE = iocshDeclWrapperTest

include $(EPICS_MODULES)/makeUtils/latest/utils.mk


EXCLUDE_VERSIONS = 3.13
BUILDCLASSES += Linux

PYTHON=python3
IOCSH=./iocshWrapper.sh

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

LOG_DIR=O.test
TESTLOG=$(addprefix $(addsuffix /,$(LOG_DIR)),test.log)

# always re-run the test
.PHONY: rmhack $(TESTLOG)

# avoid the 'clean' rule because it differs (::/:) between
# epics versions and 'driver.makefile' (::). Create a
# O. directory which is cleaned by the standard rules.

$(TESTLOG): build $(LOG_DIR)

O.%:
	mkdir -p $@

$(TESTLOG):
	$(RM) $@
	echo exit | $(IOCSH) $(addprefix -,$(TEST_EPICS)) -r iocshDeclWrapperTest -c iocInit -c 'eltc 0' test.cmd > $@


# Note: iocsh always terminates with a SIGTERM to itself;
#       therefore the test seems to fail even if it passes :-(
test: $(TESTLOG)
	$(PYTHON) checkOutput.py < $<
	echo "TEST PASSED"

pri:
	echo MODULE_LOCATION $(MODULE_LOCATION)
	echo INSTALL_DBD     $(INSTALL_DBD)
	echo INSTALL_REV     $(INSTALL_REV)
