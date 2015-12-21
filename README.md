# TUN Communication

UDP communication via TUN interface

 * setup.sh - sets up the tunnel interface and sets the rules/routes
 * runcom.c - Program to send data from stdin via TUN device


## Sending data with wrong SRC address

When sending with a wrong SRC address, the data is visible on tun16
and on eth1.
```
$ ./tuncom tun16 1.1.1.1 192.168.32.3
Press CTRL+D to terminate program.
> test1234567890
  Sent 43 bytes (15 data-bytes) via tunnel!
>

----- on second console ------

$ tcpdump -nni tun16
tcpdump: WARNING: tun16: no IPv4 address assigned
listening on tun16, link-type RAW (Raw IP), capture size 65535 bytes
04:06:13.818385 IP 1.1.1.1.12345 > 192.168.32.3.51000: UDP, length 15

----- on third console ------

$ tcpdump -nni eth1
listening on eth1, link-type EN10MB (Ethernet), capture size 65535 bytes
04:06:13.818385 IP 1.1.1.1.12345 > 192.168.32.3.51000: UDP, length 15
```

## Sending data with correct SRC address

When sending with the right SRC address, the data is only visible on tun16.

```
$ ./tuncom tun16 192.168.32.60 192.168.32.3
Press CTRL+D to terminate program.
> test1234567890
  Sent 43 bytes (15 data-bytes) via tunnel!
>

----- on second console ------

$ tcpdump -nni tun16
tcpdump: WARNING: tun16: no IPv4 address assigned
listening on tun16, link-type RAW (Raw IP), capture size 65535 bytes
04:06:13.818385 IP 1.1.1.1.12345 > 192.168.32.3.51000: UDP, length 15

----- on third console ------

$ tcpdump -nni eth1
listening on eth1, link-type EN10MB (Ethernet), capture size 65535 bytes
```
