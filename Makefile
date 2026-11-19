# Builds for 64-bit and 32-bit Wine with mingw-w64:
#   MontyShell*.exe  = taskbar + start panel + desktop + settings + window look changer (ONE program)
#   MontySetup*.exe  = installer with MontyShell embedded
CFLAGS  = -std=gnu11 -O2 -Wall -Wno-error -Wno-incompatible-pointer-types -Wno-int-conversion -Wno-unused-function -Wno-unused-variable -Wno-misleading-indentation -Wno-unused-but-set-variable
LIBS    = -luser32 -lgdi32 -lcomdlg32 -lshell32 -ladvapi32 -lmsimg32 -lkernel32
LDFLAGS = -mwindows -static -static-libgcc -s
HDRS    = common.h theme.h img.h draw.h mcfg.h mpal.h mtb1.h mtb2.h skin.h app.h ui1.h ui2.h mtv.h mst1.h mst2.h mdesk.h mmisc.h mset.h

all: MontySetup64.exe MontySetup32.exe

MontyShell64.exe: monty.c $(HDRS)
	x86_64-w64-mingw32-gcc $(CFLAGS) monty.c -o $@ $(LDFLAGS) $(LIBS)
MontyShell32.exe: monty.c $(HDRS)
	i686-w64-mingw32-gcc $(CFLAGS) monty.c -o $@ $(LDFLAGS) $(LIBS)

MontySetup64.exe: msetup.c MontyShell64.exe
	echo '1 RCDATA "MontyShell64.exe"' > res64.rc
	x86_64-w64-mingw32-windres res64.rc -O coff -o res64.o
	x86_64-w64-mingw32-gcc $(CFLAGS) msetup.c res64.o -o $@ $(LDFLAGS) -luser32 -lgdi32 -lshell32 -ladvapi32 -lkernel32
MontySetup32.exe: msetup.c MontyShell32.exe
	echo '1 RCDATA "MontyShell32.exe"' > res32.rc
	i686-w64-mingw32-windres res32.rc -O coff -o res32.o
	i686-w64-mingw32-gcc $(CFLAGS) msetup.c res32.o -o $@ $(LDFLAGS) -luser32 -lgdi32 -lshell32 -ladvapi32 -lkernel32

clean:
	rm -f *.exe *.o res*.rc
