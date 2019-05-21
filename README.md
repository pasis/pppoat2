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

Authors
-------

PPPoAT was written by Dmitry Podgorny.
