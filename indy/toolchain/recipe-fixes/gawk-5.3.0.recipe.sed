# major() and minor() are not in the headers of the cross sysroot. A header
# that every compile includes defines them; a -D with parentheses breaks in
# the shell commands of the Makefile.
s|AUTOCONF=: AUTOMAKE=: \\|printf '#define major(x) 0\\n#define minor(x) 0\\n' > "$PWD/majorminor.h"\n\tCPPFLAGS="-include $PWD/majorminor.h" AUTOCONF=: AUTOMAKE=: \\|
