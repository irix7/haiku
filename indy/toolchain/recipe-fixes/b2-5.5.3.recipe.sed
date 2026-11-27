# ./b2 is built for the target and cannot install itself here.
s|^\trunConfigure --omit-dirs "binDir" \./b2 install$|\tinstall -d "$binDir"\n\tinstall -m 755 b2 "$binDir"|
