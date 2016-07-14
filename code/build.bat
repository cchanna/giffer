@echo off

REM set CommonCompilerFlags=-MTd -nologo -Gm- -GR- -EHa- -Ox -WX -W4 -wd4201 -wd4100 -wd4189 -wd4456 -DGIFFER_INTERNAL=1 -DGIFFER_SLOW=1 -DGIFFER_WIN32=1 -FC -Z7
set CommonCompilerFlags=-MTd -nologo -Gm- -GR- -EHa- -Od -Oi -WX -W4 -wd4201 -wd4100 -wd4189 -wd4456 -DGIFFER_INTERNAL=1 -DGIFFER_SLOW=1 -DGIFFER_WIN32=1 -FC -Z7
set CommonLinkerFlags= -incremental:no -opt:ref user32.lib gdi32.lib winmm.lib
REM TODO: - can we just build both with one exe?

IF NOT EXIST ..\..\build mkdir ..\..\build
IF NOT EXIST ..\..\build\giffer mkdir ..\..\build\giffer
pushd ..\..\build\giffer
IF EXIST win32.ilk DEL /F /S /Q /A win32.ilk
IF EXIST win32.pdb DEL /F /S /Q /A win32.pdb

REM 32-bit build
REM cl %CommonCompilerFlags% ..\code\win32.cpp /link -subsystem:windows,5.1 %CommonLinkerFlags%

REM 64-bit build
del *.pdb > NUL 2> NUL
REM cl %CommonCompilerFlags% ..\giffer\code\giffer.cpp -Fmgiffer.map /MT /link /SUBSYSTEM:WINDOWS /NODEFAULTLIB:msvc
cl %CommonCompilerFlags% ..\..\giffer\code\giffer.cpp -Fmgiffer.map /c /link -incremental:no -opt:ref
lib /OUT:giffer.lib giffer.obj -EXPORT:Giffer_Init
REM-incremental:no -opt:ref  -EXPORT:Giffer_Init
REM -EXPORT:GameGetSoundSamples
REM cl %CommonCompilerFlags% ..\giffer\code\giffer_win.cpp -Fmgiffer_win32.map /I /link %CommonLinkerFlags%
popd
copy giffer.h ..\..\build\giffer\giffer.h
