@echo off
rem $Header$
title Build PalmThing
set JAVA_HOME=%~d0\jdk1.5.0_08
set ANT_HOME=%~d0\apache-ant-1.6.5
set PATH=%ANT_HOME%\bin;%JAVA_HOME%\bin;%PATH%
set CLASSPATH=
