rem
rem Build VOGL using DJGPP and NDMAKE (Call it make.exe)
rem
cd drivers
make -fmakefile.dj %1
cd ..\src
make -fmakefile.dj %1
if errorlevel == 1 goto end
cd ..\hershey\src
make -fmakefile.dj %1
if errorlevel == 1 1 goto end
if "%1" == "" goto ex
:ex
cd ..\..\examples
make -fmakefile.dj %1
:end

