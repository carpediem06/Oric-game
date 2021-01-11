gprbuild -f -P zorgonsrevenge.gpr -cargs:c -O2 -largs -mwindows 2>&1 | tee log.txt
strip --strip-debug zorgon.exe
pause
