#!/usr/bin/env bash

#
# Merge the images in data together (weight and blend)
#
# can be run with:
#    ./merge.sh | parallel -j 4
#    ffmpeg -framerate 300 -pattern_type glob -i '/tmp/*.png' -c:v libx264 -pix_fmt yuv420p /tmp/out.mp4
N=25
images=(data/*.png)
for ((i=0; i<${#images[@]}; i++)); do
    current="${images[$i]}"
    # merge N images
    others=""
    percs=""
    for ((j=$((i-N)); j<$i; j++)); do
        # we use the last N images, might be negative
        if (( $j >= 0 )); then
            if (( $j == $((i-N)) )); then
                others="${images[$j]}"
            else
                others="\( ${others} ${images[$j]} -compose Blend -define compose:args=5,95% -composite \)"
            fi
            np=$(echo "scale=4;100*e(-((($j-$i)-($N/2))*(($j-$i)-$N/2))/(2.0* ($N/2 * $N/2)))" | bc -l)
            np=$(echo "scale=4;($i-$j)-(($N+1)/2.0)" | bc -l)
            # now it goes from 2,1,0,-1,2
            np=$(echo "scale=4;80.0*e(-($np*$np)/(2.0 * ($N/5.0 * $N/5.0)))" | bc -l)
            np=$(echo "scale=0;$np/1" | bc)
            if ((j>(i-N))); then
               percs="$percs,"
            fi
            percs="$percs$np"
        fi
    done
    # now do something with this image
    num=$(printf "%04d" $i)
    #echo "$percs"
    # magick 00000150.png \( 00000051.png 00000252.png -compose Blend -define compose:args=50,50% -composite \) -compose Blend -define compose:args=50,50% -composite /tmp/erg.png
    echo "magick $others $current -compose Blend -define compose:args=5,95% -composite /tmp/erg_${num}.png"
done