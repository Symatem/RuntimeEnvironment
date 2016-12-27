#!/bin/bash

case $(uname -s) in
Darwin)
  PLATFORM=$(uname -m)-$(uname -s)-macho
  ;;
*)
  PLATFORM=$(uname -m)-$(uname -s)-elf
  ;;
esac

ARCHIVE_NAME=Symatem-${PLATFORM}.tar
PRIVATE_KEY=travis/travis_id_rsa

chmod 400 $PRIVATE_KEY

rm -f build/$ARCHIVE_NAME
(cd ./build;tar cvf $ARCHIVE_NAME SymatemMP)

echo "uploading $ARCHIVE_NAME ..."

scp -o StrictHostKeyChecking=no -i $PRIVATE_KEY ./build/$ARCHIVE_NAME mfelten_de@mfelten.de:/home/mfelten_de/docroot/Symatem/
