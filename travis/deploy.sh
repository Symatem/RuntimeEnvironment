#!/usr/bin/bash

ARCHIVE_NAME=Symatem_$(uname -s)_$(uname -p).tar

rm build/$ARCHIVE_NAME
(cd ./build;tar cvf $ARCHIVE_NAME *)

scp -i travis/travis_id_rsa ./build/$ARCHIVE_NAME mfelten_de@mfelten.de:/home/mfelten_de/docroot/Symatem/
