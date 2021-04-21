PPPoAT
======

PPPoAT - PPP over Any Transport.

PPPoAT is a modular tunneling tool. It creates a point-to-point tunnel over
a transport. Both transports and tunneling interfaces are represented by
modules.

PPP in the name refers to the first prototype which supported only PPP
interface. But now, PPPoAT supports multiple interface types and this list
can be extended with new interface modules.

Current version is the 3rd iteration with reworked architecture. Previous
version can be found at <https://github.com/pasis/pppoat>.

Examples
--------

The following example creates tunnel using pppd interface and XMPP transport.
IP addresses will be assigned by pppd automatically.

```
Server# sudo pppoat -s -i pppd -t xmpp xmpp.jid=pppoat@domain.com xmpp.passwd=password xmpp.remote=pppoat2@domain.com pppd.ip=10.0.0.1:10.0.0.2
Client# sudo pppoat -i pppd -t xmpp xmpp.jid=pppoat2@domain.com xmpp.passwd=password xmpp.remote=pppoat@domain.com
```

Check docs/ directory for more configuration examples.

Transport modules
-----------------

Transport modules are the main idea of the tool. Users can implement own modules
in order to connect two hosts when this is not possible in usual way.
Some examples of transport modules:

 * Network protocol of any layer. Including IP, TCP/UDP, XMPP, HTTP, etc.
   This kind of transports allows to connect remote hosts directly,
   bypass firewalls/NAT.
 * Physical media. For example, when hosts don't have network connectivity,
   but connected via physical port/bus or in a wireless way. Transport module
   can implement data transfer over the media and applications will be able to
   communicate through interfaces provided by interface modules.
 * Exotic modules such as FloppyNET, IP over Avian Carriers.

Interface modules
-----------------

Interface modules create well-known interfaces on both sides of the tunnel. So,
applications can use them without being modified.
List of the interface modules includes:

 * Network interfaces: PPP, TUN/TAP. With TAP interface user can configure
   a network layer protocol over it, such as IPX.
 * File descriptors: connect stdin/stdout, pipes

Intermediate modules or plugins
-------------------------------

PPPoAT introduces intermediate modules that implement such logic as data
modification, filtering, flow control. Examples are encryption, compression,
traffic shaping.

Usecases
--------

There is a wide range of usecases for PPPoAT. Some abstract examples are:

 * Bypass firewall's restrictions
 * Hide traffic via a side channel
 * Connect remote hosts to a local (broadcast) network
 * Connect hosts without network cards

More specific examples:

 * Build a tunnel with TAP interface, configure IPX over it and run old game
   with IPX support such as Warcraft2.

System requirements
-------------------

PPPoAT supports the following systems:

 * *Unix-like systems* including Linux, BSD systems, MacOS, QNX

The following systems may not be supported out of the box or have low priority:

 * *Android* - needs research, permissions may be an issue for some modules,
   engine should build and work without issues; middle priority
 * *Windows with cygwin* - needs research, interface modules may be an issue,
   engine should build and work without issues; middle priority
 * *Native Windows* support - not implemented; low priority
 * *iOS* - not implemented; low priority

PPPoAT is a cross-platform tool. If two different systems support same modules,
they will work together.

Bugs
----

Please, report bugs to the issue tracker <https://github.com/pasis/pppoat2/issues>.

Authors
-------

PPPoAT was written by Dmitry Podgorny <mailto:pasis.ua@gmail.com>.
