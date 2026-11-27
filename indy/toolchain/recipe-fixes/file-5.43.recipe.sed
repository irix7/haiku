# The magic database is built by a file of the same version that runs on the
# build machine. The installed file of the runner has another version. The
# host build comes before the configure of the source directory, which
# would make a build in another directory refuse to start.
s|^\trunConfigure \./configure$|\t(mkdir -p ../hostfile \&\& cd ../hostfile \&\& PATH=/usr/bin:/bin CC=/usr/bin/gcc CFLAGS=-O2 LDFLAGS= "$OLDPWD/configure" --disable-shared --disable-libseccomp --disable-bzlib --disable-xzlib --disable-zlib --disable-zstdlib --disable-lzlib >/dev/null \&\& PATH=/usr/bin:/bin make >/dev/null)\n\trunConfigure ./configure|
s|^\tmake$|\tmake FILE_COMPILE="$(cd .. \&\& pwd)/hostfile/src/file"|
