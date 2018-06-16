# Knowledge Services Architecture
This document is a reference guide how `EknServices` works and how it fits
into the rest of the content experience on Endless OS (such as
the [Discovery Feed](https://github.com/endlessm/eos-discovery-feed) and
[Companion App](https://github.com/endlessm/eos-companion-app-integration)).

Those components read information about content in Endless Content Apps
created using the [Endless Knowledge Framework SDK](https://github.com/endlessm/eos-knowledge-lib).

Two important design considerations exist when reading content directly
from applications.

 - **SDK Independence**: Applications may be built using any SDK verion, but
                         only a single version of the Companion App Service
                         or Discovery Feed is built. Thus, the same interface
                         needs to be exported across SDK versions and the
                         consumer of the content needs to be protected from
                         ABI breaks. The best way to ensure this is to export
                         a consistent interface over D-Bus.
 - **Memory Efficiency**: Exposing an interface over D-Bus would naturally
                          lend itself to the apps exporting the relevant
                          D-Bus interfaces automatically through
                          the Framework SDK. However, this scales very
                          poorly in practice, because a query involving
                          N apps will cause N apps to be "woken up" and
                          loaded into memory (along with N copies of gjs
                          and their resources). This would very quickly
                          cause memory exhaustion on target hardware.

Thus, `EknServicesN` is a single service capable of handling content
query requests on behalf of all applications for a given SDK version.
It does this by registering a "subtree dispatcher" on
`/com/endlessm/EknServicesN` and applications will access an object
at `/com/endlessm/EknServicesN/[encoded_app_id]` which will automatically
generate the required objects and interfaces.

# Subtree Dispatcher
When knowledge-services starts it creates a new `EksSubtreeDispatcher`
object and connects to its `dispatch-subtree` signal. Once a Session Bus
connection is created, the app calls the `register` method on it at
the `/com/endlessm/EknServicesN` object path, which in turn
will call `g_dbus_connection_register_subtree`.

That provides a single set of callbacks which tell D-Bus what to do when
certain requests are made on that object.

On enumerating chidlren of the object path, the subtree dispatcher
just returns an empty list since we use
`G_DBUS_SUBTREE_FLAGS_DISPATCH_TO_UNENUMERATED_NODES`, which tells
GDBus to try an deliver method calls to child objects it doesn't
know about anyway.

On introspecting any node in the subtree, `EknServices` reports
that all the interfaces it implements for each child object
(representing individual apps) supports every interface. This
allows callers to to interact with all exported `EknServices`
interfaces for each app (it is the caller's responsibility
to work out whether or not it should use that interface,
for instance by the installation of search provider files
and [discovery feed provider files](/docs/DiscoveryFeedProvider.md#Content Provider Files).

Finally, on invoking a method call on an interface for any
child in the subtree, the subtree dispatcher emits the
`dispatch-subtree` signal with the child name and interface
name, at which point the `EksSearchApp` takes over to create
`EksProvider` on the fly for that object and return a
`GDBusInterfaceSkeleton` capable of handling method calls
on that interface. The `GDBusInterfaceSkeleton` is based on the
`GDBusInterfaceInfo` provided at service boot-up time to
provide an implementation for that interface
(`eks_search_app_node_interface_infos`).
