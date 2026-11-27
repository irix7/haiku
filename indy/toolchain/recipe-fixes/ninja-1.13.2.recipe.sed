# getloadavg is in libbsd on Haiku.
s|python3 \./configure\.py --bootstrap|LDFLAGS="-lbsd" python3 ./configure.py --bootstrap|
