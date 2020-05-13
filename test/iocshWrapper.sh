#!/bin/bash

SSIG=true
LOADER='trap "if eval \$SSIG ; then kill -s SIGTERM 0; fi; (stty sane && echo) 2>/dev/null; rm -f $startup " EXIT ; '

IOCSH_ARGS=
while [ $# -gt 0 ] ;  do
	IOCSH_ARGS="$IOCSH_ARGS '$1'"
	shift
done

eval . `which iocsh` $IOCSH_ARGS
STAT=$?    > /dev/null
SSIG=false > /dev/null
exit $STAT
