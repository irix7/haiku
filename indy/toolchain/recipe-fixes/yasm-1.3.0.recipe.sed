# The tools that run during the build are linked by the compiler of the host.
s|^\tmake \$jobArgs$|\tmake $jobArgs CC_FOR_BUILD=/usr/bin/gcc CCLD_FOR_BUILD=/usr/bin/gcc|
