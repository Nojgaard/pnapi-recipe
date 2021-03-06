#!/usr/bin/env bash
# tests/check_customer.sh.  Generated from check_customer.sh.in by configure.
#
# this script will get a customer tool of the PNAPI,
# replace its version of the API with the present one
# and executes make check

# vim: expandtab

TOOL="$1"
LIB=${TOOL}"/libs/pnapi"

WGET="/usr/bin/wget"
TAR="$${TAR-tar}"
SRC="/home/nojgaard/Projects/mod/pnapi-4.02/src"
MAKE="/usr/bin/make"

SERVER="http://esla.informatik.uni-rostock.de:8080"
TARBALL="${SERVER}/job/${TOOL}/lastSuccessfulBuild/artifact/${TOOL}/${TOOL}.tar.gz"

if [ "$WGET" != "not found" ]; then

  # get customer tool
  ${WGET} ${TARBALL}

  # test whether file has been downloaded (otherwise there is no internet)
  if [ ! -e "${TOOL}.tar.gz" ]; then
    echo "Found no stable internet connection, so we skip some tests."
    echo ""
    exit 77
  fi

  # extract tarball
  ${TAR} xzf ${TOOL}.tar.gz

  # clear customer's library directory
  rm -rf ${TOOL}"/libs/pnapi/*"

  # copy files
  cp ${SRC}/*.cc ${SRC}/*.h ${LIB}
  cp ${SRC}/Makefile.am.customer ${LIB}/Makefile.am

  # remove unnecessary files again
  rm -rf ${LIB}/config.h

  # change to the temp dir and build tool
  cd ${TOOL}
  autoreconf -i
  ./configure --without-pnapi
  ${MAKE}
  ${MAKE} check
  result=$?
  cd ..

  echo ""

  exit $result

else

    echo "Found no wget, so we skip some tests."
    echo ""
    exit 77

fi

