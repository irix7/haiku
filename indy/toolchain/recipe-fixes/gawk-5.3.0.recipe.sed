# major() and minor() are not in the headers of the cross sysroot.
s|AUTOCONF=: AUTOMAKE=: \\|CPPFLAGS="-Dmajor(x)=0 -Dminor(x)=0" AUTOCONF=: AUTOMAKE=: \\|
