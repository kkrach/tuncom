#!/bin/sh
#
# Configures the TUN interface for tuncom
#

set -e -x

TUN="tun16"
IF="eth1"

openvpn --mktun --dev $TUN
ip link set $TUN up
ip rule add iif $TUN table 50
ip route add 0/0 dev $IF table 50

#ip rule add to 192.168.32.99 table 40
#ip route add dev tun16 table 40

