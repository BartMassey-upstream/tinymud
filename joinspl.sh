#! /bin/sh
#
# join split lines in the config.h and small.db files
#
echo creating config.h
sed -f comb.sed config_h.spl >config.h
echo creating small.db
sed -f comb.sed smalldb.spl >small.db
rm config_h.spl smalldb.spl
#
# build tinymud.ps from its three parts
#
echo creating tinymud.ps
cat tinymud.ps.xa? >tinymud.ps
rm tinymud.ps.xa?
