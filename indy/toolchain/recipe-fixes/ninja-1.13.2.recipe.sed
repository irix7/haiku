# The bootstrap of configure.py runs the ninja it has just built, which is a
# mips binary here. CMake builds it without running it. getloadavg is in
# libbsd on Haiku.
s|^\t.*python3 \./configure\.py --bootstrap|\tcmake -S. -Bbuild -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF -DCMAKE_EXE_LINKER_FLAGS=-lbsd\n\tmake -Cbuild $jobArgs ninja|
s|^\tcp ninja \$commandBinDir|\tcp build/ninja $commandBinDir|
