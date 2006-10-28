#!/bin/sh
# $Header$
export CLASSPATH=PalmThingConduit.jar:jsync.jar
java palmthing.conduit.LibraryThingImporter $*
