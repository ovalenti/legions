# legions

`legions` is a tool to generate TCP connections to a potentially large range
of faked IPs.

It achieves this by instanciating a userland TCP/IP stack for every simulated
IP and handling I/O in a completely asynchronous way (no threads).

# cheat sheet

```
# create a TUN interface and route 223.42.0.0/16 to it
sudo ip tuntap add legions mode tun user ${UID}
sudo ip addr add 223.42.0.1/16 dev legions
sudo ip link set legions up

# launch legions and instanciate a few echo services
$ ./legions legions
> help
echo <address>[-<address>] [<port>]	bind an echo service on every address (addresses created as needed)
help [<command>]	display help about commands
> echo 223.42.0.2-223.42.128.1 1234
Port: 1234
New echo instances: 32768
> 

# from the same host or any container with network access
# you can test by connecting any IP in the range
$ nc 223.42.0.2 1234
Hello !
ping
ping

```

# generator

The `generator` utility connects to IPs in a specified range, and at the
requested rate. It closes connections to keep a controlled number of
them opened in average.

```
$ ./generator --help
./generator <conn-rate> <conn-nb> <ip-range-start> <ip-range-end> <port>
	<conn-rate>	Number of new connections per second.
	<conn-nb>	The target number of connections
```

