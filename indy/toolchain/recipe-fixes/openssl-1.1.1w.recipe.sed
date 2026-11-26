# ./config guesses the build host (linux-x86_64). The target is the n32 MIPS
# Linux configuration, without the Linux-only AF_ALG engine.
s|\./config --prefix|./Configure linux-mips64 no-afalg --prefix|
