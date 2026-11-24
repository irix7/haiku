Muse
====

Muse is an operating system for the SGI Indy (IP24, R4600, big-endian mips
n32). It is a fork of [Haiku](https://www.haiku-os.org/), based on haiku/haiku
8bf39ccf93.

Muse is not affiliated with Haiku, Inc. and not endorsed by it. Haiku and the
HAIKU logo are registered trademarks of Haiku, Inc. The name appears in this
tree where it identifies the upstream project, its copyright, or a technical
interface that programs depend on: the target triple `mips64-unknown-haiku`,
package names such as `haiku.hpkg`, `uname -s`, the `__HAIKU__` macro and the
`B_HAIKU_*` API names.

State
-----
On the iris emulator the system boots from a SCSI disk image to the Tracker
desktop on the Newport display (1280 x 1024, 8 bit colour index), with PS/2
keyboard and mouse. It has not run on a physical Indy. `indy-plan.md` section
1 holds the status of every milestone.

Building
--------
`indy-plan.md` section 2 describes the workspace. The short form:

    nix develop -c ./indy/toolchain/build.sh binutils
    nix develop -c ./indy/toolchain/build.sh gcc
    ./indy/gate-m5.sh

`gate-m5.sh` builds the netboot kernel and the bootstrap image, boots them on
iris and waits for `muse: m5 userland` on the serial console.

Source layout
-------------
The tree keeps the layout of Haiku. The files of the port are listed in
`indy-plan.md` section 5. `ReadMe.Compiling.md` is the upstream build guide
and describes the other architectures.

Contributing
------------
Changes go to this repository. Upstream Haiku does not accept AI-generated
code, so nothing from this fork is sent to haiku/haiku, its Gerrit or its
issue tracker.

Licence
-------
MIT, as upstream. See `License.md`. The copyright of the Haiku code belongs to
Haiku, Inc. or to the authors named in the source files.
