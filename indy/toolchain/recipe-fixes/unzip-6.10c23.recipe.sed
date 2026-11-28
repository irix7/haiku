s|^\tmake -f unix/Makefile haiku |\tsed -i -e 's/-lbe //' -e 's/-DUNIX/-DUNIX -include wctype.h -include utime.h/g' unix/Makefile\n\tmake -f unix/Makefile haiku |
