#!/bin/bash

PLATFORM=$(uname -m)-$(uname -s)-macho
ARCHIVE_NAME=Symatem-${PLATFORM}.tar
PRIVATE_KEY=travis/travis_id_rsa

chmod 400 $PRIVATE_KEY

rm -f build/$ARCHIVE_NAME
(cd ./build;tar cvf $ARCHIVE_NAME SymatemAPI)

echo "uploading $ARCHIVE_NAME ..."

scp -i $PRIVATE_KEY ./build/$ARCHIVE_NAME mfelten_de@mfelten.de:/home/mfelten_de/docroot/Symatem/
