# runConfigure wants an optimisation flag when CFLAGS is set.
s|CFLAGS="-fPIC" runConfigure|CFLAGS="-fPIC -O2" runConfigure|
