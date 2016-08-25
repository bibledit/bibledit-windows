#!/bin/bash


# This script runs on Linux or macOS.
# It takes the source code of the Bibledit library,
# and puts it in the Windows project.
# Visual Studio can then build Bibledit for Windows.


# Work in the directory with Windows source, separate from Bibledit library source.
WINDOWSDIR=`dirname $0`
cd $WINDOWSDIR


echo Pulling in the relevant Bibledit library source code.
rsync --archive --exclude '.deps' --exclude '*.o' --exclude '*.a' --exclude '.dirstamp' --exclude 'server' --exclude 'unittest' --exclude '.DS_Store' --exclude 'autom4te.cache' --exclude 'bibledit' --exclude '*~' --exclude 'dev' --exclude 'generate' --exclude 'valgrind' --exclude 'AUTHORS' --exclude 'NEWS' --exclude 'README' --exclude 'ChangeLog' --exclude 'reconfigure' --exclude 'unittests' --exclude 'xcode' --exclude 'sources' --exclude 'DEVELOP' --exclude '*.usfm' ../bibledit/lib/* server


cd server


# Configure the Bibledit source.
./configure --enable-windows --with-network-port=9876 --enable-visualstudio
EXIT_CODE=$?
if [ $EXIT_CODE != 0 ]; then
  exit
fi


# Remove some Linux related definitions as they don't work on Windows.
sed -i.bak '/HAVE_LIBPROC/d' config.h
sed -i.bak '/HAVE_EXECINFO/d' config.h


# Remove files and folders no longer needed after configure.
find . -name ".deps" -type d -prune -exec rm -rf '{}' '+'
rm aclocal.m4
rm compile
rm config.guess
rm *.bak
rm config.h.in
rm config.log
rm config.status
rm config.sub
rm configure
rm configure.ac
rm depcomp
rm INSTALL
rm install-sh
rm Makefile*
rm missing
rm stamp-h1
rmdir unittests
rmdir sources
