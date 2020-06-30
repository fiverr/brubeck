#!/bin/bash

WD=$(dirname $(readlink -e $0))
cd $WD

EXEC='../brubeck'
if [ ! -x "$EXEC" ]; then
	echo "Error: brubeck executible not found, please run build first."
	exit 1
fi

TMP=$(mktemp -d)
VER=$(awk '/Version:/ {print $2}' control)
ARCH=$(awk '/Architecture:/ {print $2}' control)
ROOT=$TMP/brubeck-fiverr_${VER}_${ARCH}
DEB=$ROOT/DEBIAN
ETC=$ROOT/etc
LIB=$ROOT/lib
BIN=$ROOT/usr/local/bin
VAR=$ROOT/var

mkdir -p $ETC/init.d $ETC/brubeck/ $ETC/logrotate.d/ $VAR/log/brubeck/ $LIB/systemd/system/ $DEB $BIN
cp -v control postinst prerm $DEB
cp -v biz.json tech.json $ETC/brubeck/
cp -v brubeck-biz.service brubeck-tech.service $LIB/systemd/system/
cp -v logrotate $ETC/logrotate.d/brubeck
cp -v $EXEC $BIN

dpkg-deb --build $ROOT
