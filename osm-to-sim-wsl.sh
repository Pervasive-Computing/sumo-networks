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

osm_file=$1

# extract the name of the file without the extension
# and without the directory
# name=$(echo "$osm_file" | cut -f 1 -d '.')
name=$(basename "$osm_file" .osm)
dir=$(dirname "$osm_file")

file_prefix="$dir/$name"

echo "Creating SUMO network from OSM file"
netconvert.exe --osm-files "$osm_file" -o "$file_prefix.net.xml"
check_fail "netconvert"

echo "Creating SUMO routes from OSM file"
python "$SUMO_HOME/tools/randomTrips.py" -n "$file_prefix.net.xml" -e 1000 -l -r "$file_prefix.rou.xml"
check_fail "randomTrips.py"

echo "Creating SUMO polygons from OSM file"
polyconvert.exe --net-file "$file_prefix.net.xml" --osm-files="$osm_file" --type-file="typemap.xml"  -o "$file_prefix.poly.xml"
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
        <step-length value="0.01"/>
        <end value="20000"/>
    </time>
    <gui_only>
        <delay value="80"/>
        <start value="true"/>
    </gui_only>
</configuration>
EOF