#!/bin/sh
# tests/cover.sh.  Generated from cover.sh.in by configure.

#============================================================================#
# AUTOTEST WRAPPER SCRIPT FOR MAIN BINARY                                    #
# written by Niels Lohmann <niels.lohmann@uni-rostock.de>                    #
#============================================================================#

# This script has three modes:
# 1. if the environment variable COVER is set to LCOV, LCOV from the Linux
#    Test project (http://ltp.sourceforge.net/coverage/lcov.php) is used to
#    collect test case coverage results for the given executable
#
# 2. if the environment variable COVER is set to ZCOV, zcov is used to
#    collect test case coverage results for the given executable; see
#    http://minormatter.com/zcov/ for more information
#
# 3. otherwise, the given executable is called without code coverage tool


# evaluate COVER variable
case $COVER in
  "LCOV")
    # LCOV should always be quiet
    LCOV="/home/nojgaard/Projects/mod/pnapi-4.02/tests/lcov/lcov --quiet"

    # derive test name from current directory (assuming GNU Autotest); the
    # info filename is suffixed with a random number to allow for multiple
    # invokations of this script for the same test
    dir=`pwd`
    TESTNAME=`basename $dir`
    INFOFILE=/home/nojgaard/Projects/mod/pnapi-4.02/tests/cover/$TESTNAME-$RANDOM.info

    # reset counters for LCOV
    $LCOV --zerocounters -directory /home/nojgaard/Projects/mod/pnapi-4.02

    # execute executable and remember exit code
    $*
    result=$?

    # evaluate coverage result of LCOV
    $LCOV --capture --directory /home/nojgaard/Projects/mod/pnapi-4.02 --output-file $INFOFILE --test-name $TESTNAME >/dev/null 2>&1

    # remove profile information on C++ sources and headers
    $LCOV --extract $INFOFILE "/home/nojgaard/Projects/mod/pnapi-4.02/*" -o $INFOFILE

    # exit with executable's exit code
    exit $result  
  ;;

  "ZCOV")
    # set ZCOV variable
    ZCOV="/home/nojgaard/Projects/mod/pnapi-4.02/tests/zcov/zcov-scan"

    # derive test name from current directory (assuming GNU Autotest); the
    # info filename is suffixed with a random number to allow for multiple
    # invokations of this script for the same test
    dir=`pwd`
    TESTNAME=`basename $dir`

    # execute executable and remember exit code
    $*
    result=$?

    # scan directory
    $ZCOV /home/nojgaard/Projects/mod/pnapi-4.02/tests/cover/$TESTNAME.zcov /home/nojgaard/Projects/mod/pnapi-4.02 &> /dev/null

    # exit with executable's exit code
    exit $result  
  ;;

  *)
    $*
    exit $?
  ;;
esac
