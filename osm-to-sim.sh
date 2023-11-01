#!/bin/bash

# function for checking if a command failed
# and exiting if it did
function check_fail {
    if [ $? -ne 0 ]; then
        echo "$1 failed"
        exit 1
    fi
}

# make sure SUMO_HOME is set
if [ -z "$SUMO_HOME" ]; then
    echo "SUMO_HOME is not set"
    exit 1
fi

# take an OSM file as input
if [ -z "$1" ]; then
    echo "No OSM file specified"
    exit 1
fi

# Check if running in WSL
if grep -q microsoft /proc/version; then
    is_wsl=1
else
    is_wsl=0
fi

netconvert_binary="netconvert"
polyconvert_binary="polyconvert"

# Check if running in WSL
if [ $is_wsl -eq 1 ]; then
    echo "Running in WSL"
    echo "Adding .exe to netconvert and polyconvert"
    netconvert_binary="$netconvert_binary.exe"
    polyconvert_binary="$polyconvert_binary.exe"
fi

osm_file=$1

# extract the name of the file without the extension
# and without the directory
# name=$(echo "$osm_file" | cut -f 1 -d '.')
name=$(basename "$osm_file" .osm)
dir=$(dirname "$osm_file")

file_prefix="$dir/$name"

echo "Creating SUMO network from OSM file"
$netconvert_binary --osm-files "$osm_file" -o "$file_prefix.net.xml" --junctions.join --no-left-connections --tls.discard-simple --tls.default-type actuated --no-turnarounds
# --default.junctions.keep-clear
# --osm.bike-access --osm.sidewalks --osm.crossings --osm.turn-lanes
# --tls.guess-signals --tls.guess.joining --tls.rebuild --tls.join --tls.join-dist 100.0 --tls.discard-simple --tls.default-type actuated --tls.ignore-internal-junction-jam --tls.group-signals --tls.left-green.time 10
# --junctions.minimal-shape --junctions.join --junctions.join-turns --junctions.join-dist 20.0 --junctions.join-same
# --ramps.guess
# --geometry.remove --geometry.avoid-overlap
# --no-left-connections
# --no-turnarounds --no-turnarounds.except-deadend
check_fail "netconvert"

echo "Creating SUMO routes from OSM file"
python "$SUMO_HOME/tools/randomTrips.py" -n "$file_prefix.net.xml" --random-routing-factor 2.0 --insertion-density 100 -e 20000 -L -r "$file_prefix.rou.xml"
check_fail "randomTrips.py"

typemap_filename="typemap.xml"

echo "Checking if typemap.xml exists in current directory"
if [ ! -f "typemap.xml" ]; then
    echo "typemap.xml does not exist in current directory"
    echo "Copying typemap.xml from $SUMO_HOME/data/typemap/osmPolyconvert.typ.xml"
    cp "$SUMO_HOME/data/typemap/osmPolyconvert.typ.xml" "$typemap_filename"
fi

echo "Creating SUMO polygons from OSM file"
$polyconvert_binary --net-file "$file_prefix.net.xml" --osm-files="$osm_file" --type-file="$typemap_filename"  -o "$file_prefix.poly.xml"
check_fail "polyconvert"

echo "Creating SUMO configuration file"
cat << EOF > "$file_prefix.sumocfg"
<configuration>
    <input>
        <net-file value="$name.net.xml"/>
        <route-files value="$name.rou.xml"/>
        <additional-files value="$name.poly.xml"/>
    </input>
    <time>
        <begin value="0"/>
        <step-length value="0.1"/>
        <end value="20000"/>
    </time>
    <gui_only>
        <delay value="0"/>
        <start value="true"/>
    </gui_only>
</configuration>
EOF
