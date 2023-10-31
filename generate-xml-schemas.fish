#!/usr/bin/env fish

set --global postfix schema.xsd
set --global SCHEMAS_DIR ./xml-schemas

set reset (set_color normal)
set green (set_color green)
set red (set_color red)
set yellow (set_color yellow)

mkdir -p $SCHEMAS_DIR

set f $SCHEMAS_DIR/sumocfg.$postfix
if not test -f $f
    printf "Generating %s%s%s\n" $green $f $reset
    sumo-gui --save-commented=true --save-schema=$SCHEMAS_DIR/sumocfg.$postfix
else
    printf "Skipping %s%s%s\n" $yellow $f $reset
end

set f $SCHEMAS_DIR/netedit.$postfix
if not test -f $f
    printf "Generating %s%s%s\n" $green $f $reset
    netedit --save-commented=true --save-schema=$SCHEMAS_DIR/netedit.$postfix
else
    printf "Skipping %s%s%s\n" $yellow $f $reset
end

set f $SCHEMAS_DIR/netconvert.$postfix
if not test -f $f
    printf "Generating %s%s%s\n" $green $f $reset
    netconvert --save-commented=true --save-schema=$SCHEMAS_DIR/netconvert.$postfix
else
    printf "Skipping %s%s%s\n" $yellow $f $reset
end






