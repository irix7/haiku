# The compiler snowball runs during the build. A host build of the whole
# package makes the generated sources; the build for the target takes them.
s|^\tmake \$jobArgs$|\t(cp -a . ../hostsnowball \&\& cd ../hostsnowball \&\& PATH=/usr/bin:/bin make CC=/usr/bin/gcc >/dev/null)\n\tmake $jobArgs snowball\n\tmkdir -p src_c\n\tcp ../hostsnowball/src_c/* src_c/\n\ttouch src_c/*\n\tmake $jobArgs|
