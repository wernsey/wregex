@echo off
rem Batch file to build with the Borland C++ 5.5
rem command line compiler
rem THIS SCRIPT IS NOT MAINTAINED

if "%1" == "clean" goto clean
if "%1" == "wipe" goto wipe

rem -- Compiler steps --
bcc32 -owrx_comp.obj -c wrx_comp.c
bcc32 -owrx_exec.obj -c wrx_exec.c
bcc32 -owrx_err.obj -c wrx_err.c
bcc32 -owrx_free.obj -c wrx_free.c
bcc32 -owrx_prnt.obj -c wrx_prnt.c
bcc32 -otest.obj -c test.c
bcc32 -otest.obj -c test.c
bcc32 -ogetarg.obj -c getarg.c
bcc32 -owgrep.obj -c wgrep.c

rem -- Linker steps --
bcc32 -eTest.exe test.obj wrx_comp.obj wrx_exec.obj wrx_err.obj wrx_free.obj wrx_prnt.obj
bcc32 -ewgrep.exe wgrep.obj wrx_comp.obj wrx_exec.obj wrx_err.obj wrx_free.obj wrx_prnt.obj getarg.obj

goto end
:clean
rem -- Delete all output files --
del *.exe
:wipe
rem -- Delete intermediate files --
del *.obj
del *.tds
:end