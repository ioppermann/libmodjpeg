#!/bin/sh

#betterjpeg.sh
#Depends: dash, graphicsmagick, modjpeg

sname="BetterJpeg"
sversion="1.0.0"

echo "$sname $sversion"

tnocomp=""
tcomp="gm"
[ ! "$(command -v $tcomp)" ] && tnocomp="$tnocomp $tcomp"
tcomp="modjpeg"
[ ! "$(command -v $tcomp)" ] && tnocomp="$tnocomp $tcomp"
if [ "x$tnocomp" != "x" ]
then
    echo "Not found:${tnocomp}!"
    echo ""
    exit 1
fi

if [ -d /tmp ]
then
    TEMP=/tmp
else
    TEMP=.
fi
    
fhlp="false"
thres="1"
while getopts ":t:h" opt
do
    case $opt in
        t) thres="$OPTARG"
            ;;
        h) fhlp="true"
            ;;
        *) echo "Unknown option -$OPTARG"
            exit 1
            ;;
    esac
done
shift "$(($OPTIND - 1))"
src="$1";
mod="$2";
dst="$3";

if [ "x$src" = "x" -o "x$mod" = "x" -o "x$dst" = "x" -o "x$fhlp" = "xtrue" ]
then
    echo "Usage:"
    echo "$0 [options] original.jpg modify.png result.jpg"
    echo "Options:"
    echo "    -t N  threshold (default = 1)"
    echo "    -h    help"    
    exit 0
fi

if [ ! -f "$src" ]
then
    echo "Not found $src!"
    exit 1
fi

if [ ! -f "$mod" ]
then
    echo "Not found $mod!"
    exit 1
fi

gm composite -compose Difference "$src" "$mod" "$TEMP/betterjpeg.d.$$.png"
gm mogrify -threshold "$thres" -negate "$TEMP/betterjpeg.d.$$.png"
gm mogrify -transparent white "$TEMP/betterjpeg.d.$$.png"
gm composite -compose Atop "$mod" "$TEMP/betterjpeg.d.$$.png" "$TEMP/betterjpeg.s.$$.png"
rm "$TEMP/betterjpeg.d.$$.png"
modjpeg -i "$src" -d "$TEMP/betterjpeg.s.$$.png" -o "$dst"
rm "$TEMP/betterjpeg.s.$$.png"
