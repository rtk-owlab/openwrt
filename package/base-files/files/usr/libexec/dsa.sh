#!/bin/sh

. /usr/share/libubox/jshn.sh

case "$1" in
        list)
                echo '{ "getSwconfigPortState": {"switch": "str"}, "getSwconfigFeatures": {"switch": "str"} }'
        ;;
        call)
                case "$2" in
                        getSwconfigPortState)
                                # read the arguments
                                read input;
                                
                                json_init
                                
                                json_add_array result
                                for net in `ls -d /sys/class/net/*`; do
                                        switchid=`cat $net/phys_switch_id 2>/dev/null`
                                        [ -z "$switchid" ] && continue
                                        device=`basename $net`
                                        portid=`cat $net/phys_port_name`
                                        json_add_object
                                                json_add_int port $portid
                                                duplex=`echo $net/duplex`
                                                json_add_boolean duplex `expr $duplex=true` 
                                                json_add_int speed `cat $net/speed`
                                                json_add_boolean link `cat $net/carrier`
                                        json_close_object
                                done
                                for net in `ls -d /sys/class/net/*/dsa`; do
                                        master=`dirname $net`
                                        portid=`cat $master/phys_port_name`
                                        json_add_object
                                                json_add_int port $portid
                                                duplex=`echo $master/duplex`
                                                json_add_boolean duplex `expr $duplex=true` 
                                                json_add_int speed `cat $master/speed`
                                                json_add_boolean link `cat $master/carrier`
                                        json_close_object
                                done

                                json_dump
                        ;;
                        getSwconfigFeatures)
                                read input
                                echo $input
                                json_init
                                json_add_string         "switch_title" "eth0"
                                json_add_boolean        "dsa" 1
                                json_add_int            "num_vlans" 4095
                                json_add_string         "mirror_option" "enable_mirror_rx"
                                json_add_boolean        "enable_vlan" 0
                                json_add_string         "vlan4k_option" "enable_vlan4k"
                                json_add_int            "min_vid" 1
                                json_close_object
                                json_dump
                        ;;
                        failme)
                                # return invalid
                                echo '{asdf/3454'
                        ;;
                esac
        ;;
esac
