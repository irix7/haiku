# libffi has no configuration for haiku. The mips Linux one has the n32 ABI
# and the same calling convention.
s|runConfigure \./configure --disable-static|runConfigure ./configure --disable-static --host=mips-unknown-linux-gnu|
