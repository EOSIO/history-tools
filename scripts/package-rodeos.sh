#!/bin/bash
set -e
cd "$(git rev-parse --show-toplevel)/build"
# construct version string
[[ -z "$PACKAGE_REVISION" ]] && export PACKAGE_REVISION='1'
[[ -z "$VERSION_STRING" ]] && export VERSION_STRING="$(./rodeos --version | sed 's/^v//' | sed "s/$/-$PACKAGE_REVISION/")"
# determine file size
export INSTALLED_SIZE_KB="$(du -k rodeos | cut -f 1)"
# define binary install path
mkdir -p rodeos_$VERSION_STRING/usr/local/bin
mv rodeos rodeos_$VERSION_STRING/usr/local/bin/
# create dpkg control file
mkdir rodeos_$VERSION_STRING/DEBIAN
export CONTROL_FILE="rodeos_$VERSION_STRING/DEBIAN/control"
echo 'Package: rodeos' > $CONTROL_FILE
echo "Version: $VERSION_STRING" >> $CONTROL_FILE
echo 'Architecture: all' >> $CONTROL_FILE
echo 'Maintainer: block.one <support@block.one>' >> $CONTROL_FILE
echo 'Depends: libatomic1, libssl1.1' >> $CONTROL_FILE
echo "Installed-Size: $INSTALLED_SIZE_KB" >> $CONTROL_FILE
echo 'Homepage: https://github.com/EOSIO/history-tools' >> $CONTROL_FILE
echo 'Description: Part of EOSIO, connects nodeos state-history plugin to your database' >> $CONTROL_FILE
cat $CONTROL_FILE
# ensure correct permissions
chown root:root -R rodeos_$VERSION_STRING
chmod 0755 rodeos_$VERSION_STRING/usr/local/bin/rodeos
# package build
dpkg -b rodeos_$VERSION_STRING