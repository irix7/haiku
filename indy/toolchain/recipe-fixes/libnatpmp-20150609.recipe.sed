# socket and inet_ntoa are in libnetwork.
s|^\tmake \$jobArgs$|\tmake $jobArgs LDFLAGS=-lnetwork|
