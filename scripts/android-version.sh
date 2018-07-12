#!/bin/sh

MAJOR=$(echo $1 | cut -d '.' -f 1)
MINOR=$(echo $1 | cut -d '.' -f 2)
PATCH=$(echo $1 | cut -d '.' -f 3)
if [ "x$PATCH" != "x" ] ; then
  printf "%d%02d%02d\\n" $MAJOR $MINOR $PATCH
else
  printf "%d%02d00\\n" $MAJOR $MINOR
fi
