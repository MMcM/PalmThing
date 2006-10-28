@echo off
rem $Header$
setlocal
set PATH={JSync}\rt\bin;%PATH%
set CLASSPATH={app}\PalmThingConduit.jar;{JSync}\jsync.jar
java palmthing.conduit.LibraryThingImporter %*
endlocal
