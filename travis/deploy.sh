#!/bin/bash

ARCHIVE_NAME=Symatem_$(uname -s)_$(uname -p).tar
PRIVATE_KEY=travis/travis_id_rsa

echo "uploading $ARCHIVE_NAME ..."

chmod 400 $PRIVATE_KEY

rm build/$ARCHIVE_NAME
(cd ./build;tar cvf $ARCHIVE_NAME *)

scp -i $PRIVATE_KEY ./build/$ARCHIVE_NAME mfelten_de@mfelten.de:/home/mfelten_de/docroot/Symatem/
