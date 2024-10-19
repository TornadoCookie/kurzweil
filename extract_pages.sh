#!/bin/bash

for dir in $1/*;
do
    BM_PATH="$dir/Text Bitmap"
    DEST_PATH=$2/$(basename "$dir").tiff
    echo "BM_PATH " $BM_PATH
    echo "DEST_PATH " $DEST_PATH

    cp "$BM_PATH" "$DEST_PATH"
done
