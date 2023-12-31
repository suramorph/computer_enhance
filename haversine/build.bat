@echo off

ctime -begin haversine.ctm

set CommonCompilerFlags=-Od -MTd -nologo -fp:fast -fp:except- -GR- -EHa- -Zo -Oi -WX -W4 -wd4505 -wd4996 -wd4244 -wd4201 -wd4100 -FC -Z7 -DDEBUG

set CommonCompilerFlags= %CommonCompilerFlags% 
set CommonLinkerFlags= -incremental:no

IF NOT EXIST bin mkdir bin
pushd bin
cl %CommonCompilerFlags% ..\source\winmain.cpp /link %CommonLinkerFlags%
set LastError=%ERRORLEVEL%
popd

ctime -end haversine.ctm %LastError%