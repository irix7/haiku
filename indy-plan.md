# Haiku on the SGI Indy

Plan for porting Haiku to the SGI Indy (IP24) as a new architecture named `mips`.
The target machine is an Indy with an R4600 CPU, and every decision in section 4
is chosen for that CPU. The stock PROM netboots the loader with
`boot -f bootp()haiku`. The base is haiku/haiku 8bf39ccf93.

Section 3 was checked on 2026-11-17 against this tree, the `../iris` source, the
`../os` and `../redox` code, and two boots on iris (section 7). Facts that have
not been confirmed on a physical Indy are marked "hardware unverified".

## 0. How to use this plan

Readers are coding agents working one task at a time, and the owner.

Read sections 0 to 5 once. After that, read the milestone you are working on in
section 8 and the parts of section 3 it names.

Each task card has three parts. "Read" lists files to open first. "Do" lists the
work. "Done when" is a command and the output that proves the task.

Rules:

1. Work on one task card per session. Stop when "Done when" is met.
2. A task is done when its check prints the stated output. Record the command
   and the output line in section 1. A task with no recorded output is open.
3. Take hardware addresses and bit values from section 3. If section 3 lacks a
   value, read it from the source named there, add it to section 3 with the
   file and line, then use it. Do not guess a register value.
4. When a card names a template file, copy its structure. Change only what the
   card and the MIPS facts require.
5. Change generic (non-MIPS) files only where section 5.2 or a card names them.
6. Do not change a decision in section 4. Write the objection in section 9 and
   continue with another task.
7. After three failed attempts at the same error, stop. Write the error, what
   was tried and the log path in section 9.
8. Upstream Haiku does not accept AI-generated code (`AGENTS.md` at HEAD). All
   work stays in this fork. Do not push to `origin`, do not open Gerrit changes
   or upstream tickets. Do not add AI attribution to commits.
9. Write new code. Copying is allowed from this tree and from `../redox` (both
   MIT). `../os` is GPL-3.0 and Linux is GPL-2.0: read them for facts only.
10. Build under `generated.mips/` and `/mnt/europa/haiku-toolchain/`. The root
    filesystem has under 8 GB free and must not hold build trees.
11. Match the style of the file you edit: tabs, Haiku naming, 80 columns.
12. Keep this file current: section 1 after every task, section 9 for every
    trap found.

## 1. Status

| Milestone | Gate marker | State |
|---|---|---|
| M0 gate rig | `haiku: m0 alive` on both gates | Done 2026-11-18. `nix develop -c jam -v` prints `Jam 2.5-haiku-20211029`. `make -C indy gate-m0` and `gate-gfx-m0` print `PASS`. `elf2ecoff.py` output equals the Redox tool's byte for byte. |
| M1 toolchain and build wiring | `configure` accepts `mips-unknown-haiku-gcc` | Done 2026-11-18. binutils 2.47 and GCC 17 are installed in `cross-tools-mips`, with libgcc for both multilibs, libstdc++, and the kernel libraries of `build.sh kernel-libs`. `seed.sh` fills the seed directory. `../configure --cross-tools-prefix ...` prints `Configured successfully!`. |
| M2 loader | `haiku_loader: archive mounted` | Done 2026-11-18 on iris. The serial gate shows `haiku_loader: start`, the Count rate (33.0 MHz), `RAM: 64 MB`, `haiku_loader: archive mounted`, `haiku_loader: kseg2 ok`, then the kernel's first line. |
| M3 kernel core | `haiku: m3 threads ok` | Gate passes on iris 2026-11-18: `./indy/gate.sh generated.mips/haiku-indy-netboot "haiku: m3 threads ok"`. User mode, FPU and signal code have not run. |
| M4 modules, interrupts, storage | `haiku: m4 boot volume mounted` | M4.1 to M4.4 work on iris 2026-11-18: add-ons load, `wd33c93: controller ready`, the disk is published as `disk/scsi/0/1/0/raw`, and with a BFS image the log shows `bfs: mounted "Haiku"` and `Mounted boot partition`. The boot then stops at `error starting "/boot/system/servers/launch_daemon"`, because the volume is empty. M4.5 (interrupts, DMA) not written. |
| M5 userland | `haiku: m5 userland` | Arch files written (M5.1 to M5.3). `libroot.so` and `runtime_loader` link (section 1.4). M5.4 done 2026-11-24 (section 1.3). M5.5 and M5.6 not started. |
| M6 desktop | `haiku: m6 desktop` plus screenshot | M6.1 done on iris 2026-11-18 (`make -C indy gate-rex3`, both gradients drawn). Driver, accelerant, app_server hook, isa and ps2 changes written, not run. |
| M7 network | `haiku: m7 ping ok` | The Seeq driver works on iris 2026-11-18: the kernel configures the interface and mounts its boot volume from a RemoteDisk server (D10a). DHCP and ping from userland not run. |
| M8 physical Indy | same markers on the machine's serial port | Not started |
| M9 performance on the R4600 | measured gains recorded per task | Not started |

### 1.1 State of the tree on 2026-11-18

Every file of section 5 exists in the working tree, uncommitted, except where
this list says otherwise. The loader, the kernel and the boot modules build
with the real toolchain and run on iris up to the mounted boot volume (table
above). The userland arch files, the Newport driver and accelerant, the
app_server hook, the isa and ps2 changes and the Seeq driver have only been
parsed with a stand-in compiler: expect compile errors and run-time bugs
there. For those, a task card means: build the files, run the gate, fix what
fails.

Differences from the file list of section 5:

- `asm_offsets.cpp` does not exist. Assembly code uses the layout macros of
  `arch_exception_block.h`, and `arch_int.cpp` and `arch_thread.cpp` check
  them with `static_assert`.
- `hpc3_dma.cpp` does not exist. The SCSI driver is `wd33c93.cpp` and
  `wd33c93.h`, polled PIO only.
- The Newport kernel driver is one file, `driver.cpp`. The accelerant is
  `accelerant.cpp`, `accelerant.h` and `upload.cpp`. `cursor.cpp` (M9.4) does
  not exist.
- The Seeq driver is `driver.cpp` and `seeq8003.h`.
- `src/system/boot/platform/arcs/entry.S` also holds the loader's TLB refill
  handler, TLB init and cache flush.
- Added: `indy/toolchain/patches/binutils-haiku-mips.patch` and
  `gcc-haiku-mips.patch`, `headers/private/kernel/arch/elf.h` (prototype),
  `src/system/kernel/lib/strtod.c` (`__mips__` in the byte order list).
- The GCC worktree `/mnt/europa/haiku-toolchain/src/gcc` exists on branch
  `haiku-mips` with the patch applied and nothing committed.
- haikuporter and haikuports.cross are not cloned. Their changes are not
  written.

How the working state is reproduced (all inside `nix develop`; until
`flake.nix` is tracked by git, copy `flake.nix` and `flake.lock` to an empty
directory and use `nix develop path:THAT_DIRECTORY`):

```
indy/toolchain/build.sh all          # binutils, GCC, target libraries, kernel libraries
indy/toolchain/seed.sh               # seed directory
cd generated.mips
../configure --cross-tools-prefix /mnt/europa/haiku-toolchain/cross-tools-mips/bin/mips-unknown-haiku-
jam -q -sHAIKU_REVISION=hrev59000 haiku-netboot-indy     # loader + kernel + boot modules
jam -q -sHAIKU_REVISION=hrev59000 "<build>bfs_shell"
truncate -s 64M disk.raw
LD_LIBRARY_PATH=$(dirname $(find objects/linux -name libroot_build.so)) \
  objects/linux/x86_64/release/tools/bfs_shell/bfs_shell --initialize disk.raw Haiku
cd ..
DISK=generated.mips/disk.raw ./indy/gate.sh generated.mips/haiku-indy-netboot "Mounted boot partition" 90
```

What the first real builds and boots changed (all in the tree now):

- The clone has no `hrev` tags, so jam needs `-sHAIKU_REVISION=hrev59000`.
- `headers/config/HaikuVersion.h` accepts GCC 17.
- `-fno-pic` alone still gives position independent MIPS code. The kernel and
  loader flags are `-fno-pic -mno-shared -mplt` (`ArchitectureRules`). Jam does
  not rebuild objects when flags change: delete `objects/haiku/mips` after a
  flag change.
- The kernel and loader link the libraries of `build.sh kernel-libs`: libgcc
  and libsupc++ rebuilt freestanding, without threads and TLS, as the
  HaikuPorts gcc recipe does.
- binutils: `TE_TMIPS` in `gas/config/te-haiku.h` and the emulation name in
  `elf32bmipn32-defs.sh`. GCC: `MIPS_DEFAULT_GVALUE 0`, and the `wcsftime` cast
  of the fork made IRIX-only.
- `src/system/boot/loader/kernel_args.cpp` aligns allocations to 8 bytes on
  mips, and the kernel emulates unaligned loads and stores in kernel mode
  (`emulate_unaligned_access` in `arch_int.cpp`). Generic kernel code reads
  packed boot structures through member pointers.
- Assembly files shared with the kernel use the FPU only in userland builds
  (`USE_FPU` in `sigsetjmp.S`, `siglongjmp.S`, `byteorder.S`).
- `scsi_disk` is a boot module in `NetBootArchive` for mips.
- `vm_block_address_range()` returns an area ID.

Known noise and open points from these runs:

- The loader prints "unhandled pheader type 0x70000000" and "0x0" many times
  while it loads images (the MIPS ABI flags and register info headers).
- ld warns that kernel add-ons mix hard-float glue objects (`crtbeginS.o`) with
  soft-float code. The add-ons load and run.
- libstdc++'s C++23 module `std.cc` does not compile ("not declared in std" for
  24 `long double` functions). The library builds and installs without it.

Design points that were fixed while writing the code, and that the decisions
of section 4 now include: the page table entry format (D9), the `mips` macro
(section 9), the REX3 state reset (3.8), the data cache write-back on
executable mappings (section 9).

### 1.2 Kernel debugger and GDB stub, 2026-11-24

Not a milestone of section 8. The kernel has a GDB stub
(`src/system/kernel/debug/gdb.cpp`, generic, with mips hooks in
`arch_debug.cpp`) on two channels: serial port 1, and UDP port 2159 on the
Seeq controller (`arch_debug_net.cpp`, polled inside the kernel debugger; the
seeq8003 driver hands it the datagrams while the kernel runs).

- Enter from the serial port: Control-C stops the kernel at the KDL prompt,
  then GDB's first packet starts the stub. Enter from the network: the first
  datagram to port 2159 stops the kernel.
- GDB: `target remote udp:ADDRESS:2159`, or the serial line. No MIPS GDB is
  installed on this host. The gate uses `indy/gdb-check.py`, a client for the
  remote protocol.
- Supported: `?`, `g`, `G`, `p`, `P`, `m`, `M`, `Z0`, `z0`, `c`, `s` (through
  temporary breakpoints at the next pc and the branch target), thread list,
  `Hg`, `D`, `k`.

Checked on iris 2026-11-24, inside `nix develop`:

```
./indy/gate-gdb.sh serial     # gdb-check: PASS, gate: PASS
./indy/gate-gdb.sh udp        # gdb-check: PASS, gate: PASS
DISK=generated.mips/disk.raw BOOT_ARGS=disk ./indy/gate.sh \
  generated.mips/haiku-indy-netboot "Mounted boot partition"   # gate: PASS
```

Bugs these runs found and fixed:

- `arch_debug_call_with_fault_handler()` ended with `longjmp(jumpBuffer, 1)`
  after a function that returned normally. Every KDL command was reported as
  faulted, `debug_memcpy()` always took its slow path, and a command's
  `B_KDEBUG_QUIT` was lost. It returns now.
- The fault handler of the kernel debugger kept its jump buffer in one static
  pointer, which a nested call left pointing at a dead frame. The buffer is
  now found through `cpu_ent::fault_handler_stack_pointer`
  (`mips_debug_fault_link`), which the generic code restores. Address errors
  and bus errors inside the debugger use the same handler.
- The KDL prompt consumed the `$` of GDB's first packet, and the stub waited
  for the next one. The prompt now runs `gdb packet`.

Not tested: a real GDB, the network console on the physical Indy (hardware
unverified), breakpoints in user teams.

### 1.3 M5.4 shared GCC libraries, 2026-11-24

`indy/toolchain/syslibs.sh /mnt/europa/haiku/generated.mips` (inside
`nix develop`) builds a second GCC tree with `--enable-shared` in
`/mnt/europa/haiku-toolchain/build/gcc-shared` and writes two packages to
`/mnt/europa/haiku-toolchain/build/syslibs`. `syslibs.sh OUTPUT package`
repeats the packaging only. The log is `build/syslibs.log`.

`package list` output, libraries only:

```
gcc_syslibs-17.0.0_indy-1-mips.hpkg (5.0 MB)
  lib/libgcc_s.so.1, libgcc_s.so, libstdc++.so.6.0.37, libstdc++.so.6,
  libstdc++.so, libsupc++.so
gcc_syslibs_devel-17.0.0_indy-1-mips.hpkg (25.9 MB)
  develop/lib/libgcc.a, libgcc_eh.a, libstdc++.a, libsupc++.a,
  libgcc-kernel.a, libgcc_eh-kernel.a, libsupc++-kernel.a,
  libgcc-boot.a, libgcc_eh-boot.a, libsupc++-boot.a
  develop/headers/c++, develop/headers/gcc/include
```

How it differs from the card:

- The stub libroot is compiled by the script from
  `src/system/libroot/stubbed/libroot_stubs.c`. Its jam target needs a build
  configured with `--bootstrap`. The start files (`crti.o`, `crtn.o`,
  `start_dyn.o`, `init_term_dyn.o`) come from jam in `generated.mips`: this
  was the first compile of the mips glue with the real toolchain.
- The sysroot `cross-tools-mips/sysroot/boot/system/develop/lib` now holds
  the stub `libroot.so` and the start files, so the installed cross compiler
  can link shared objects against them.
- `libsupc++.so` is linked by the script from `libsupc++convenience.a`. The
  GCC build does not produce it.
- The kernel and boot variants are the libraries of `build.sh kernel-libs`.
  `libgcc_eh-kernel.a` and `libgcc_eh-boot.a` are empty archives.
- The shared libraries are stripped of debug information.

Not done: the packages are not in a repository and jam still uses the seed
directory. No program has been linked against these libraries or run with
them.

### 1.4 M5.2 and M5.3 link checks, 2026-11-24

`./indy/jam.sh libroot.so` and `./indy/jam.sh runtime_loader` both end with
`jam.sh: exit 0` with the real toolchain.

- `libroot.so`: 3.1 MB, flags `abi2, mips3`. Its only undefined symbol is the
  weak `__cxa_pure_virtual`. Dynamic relocations: 1739 `R_MIPS_REL32` and one
  `R_MIPS_TLS_DTPMOD`.
- `runtime_loader`: 265 kB, no undefined symbols, 211 `R_MIPS_REL32`, two
  load segments.

What the first build of libroot changed:

- `musl/internal/libm.h` declares `exp10()`, `sincos()` and `__fpclassify()`.
  Haiku's `math.h` does not, and GCC 14 and later reject the implicit
  declarations in `exp10l.c`, `sincosl.c` and `__fpclassifyl.c`. These files
  call the double functions where long double is double (D1).
- `glibc/stdlib/longlong.h`: the mips `umul_ppmm` used the asm constraint
  `h`, which GCC 4.4 removed. It is a 64-bit multiplication in C now.
- `musl/math/mips/Jamfile` lists `__fpclassify.c`, `__fpclassifyf.c` and
  `__fpclassifyl.c`, as the arm Jamfile does.

Neither binary has run. The kernel has to apply the 211 relocations of
`runtime_loader` when it loads it (M5.1, not exercised).

### 1.5 M5.5 third-party packages, in progress 2026-11-24

Setup that works:

- Clones under `/mnt/europa/haiku-toolchain/ports`: haikuporter (c078118),
  haikuports.cross (a6e2c64), haikuports (569482e, depth 1): the upstream
  commits the work started from. Since 2026-11-24 the clones are on branch
  `indy-mips` of the forks `github.com/irix7/haikuporter` and
  `irix7/haikuports.cross` (remote `irix7`), where that branch is the default
  branch, as it is in `irix7/haiku`. Commit recipe changes there. The GCC and
  binutils worktrees are on branch `haiku-mips` of `irix7/gcc` and
  `irix7/binutils-gdb`, committed and pushed.
- haikuporter: `mips` with triplet `mips-unknown-haiku` in `RecipeTypes.py`.
  Two fixes for the NixOS host: git gets `PATH` (`Source.py`), and the build
  shells keep `PATH` (`Utils.py` passes `NOSYSBASHRC`, which `port.sh` sets).
- haikuports.cross: `mips` in `ARCHITECTURES` of the recipes that list
  architectures by name. Recipes with `all` need no change.
- The bootstrap build directory is `/mnt/europa/haiku-toolchain/bootstrap`,
  configured with `--cross-tools-prefix` and `--bootstrap`. jam needs the
  profile to select the bootstrap build type:
  `OUTPUT=/mnt/europa/haiku-toolchain/bootstrap ./indy/jam.sh @bootstrap-raw build TARGET`.
- `BOOTSTRAP_OUTPUT=... indy/toolchain/syslibs.sh generated.mips package`
  puts `gcc_bootstrap_syslibs` and `gcc_bootstrap_syslibs_devel` into the
  repository's `packages` directory, so haikuporter does not build GCC. jam
  extracts them to `bootstrap/build_packages`: delete that copy after
  repackaging.
- `haiku_cross_devel_sysroot_stage1_mips.hpkg` builds. This compiled and
  linked libroot, libbsd and libnetwork for mips in the bootstrap tree.
- `indy/toolchain/port.sh PORT...` runs haikuporter for the named ports.
  jam's own rule builds the packages in the order of the repository file.

Packages in
`bootstrap/objects/haiku/mips/packaging/repositories/HaikuPortsCross-build/packages`
on 2026-11-24: `ca_root_certificates`, `haikuporter`, `noto` (three
packages), `zlib_bootstrap-1.3.2-1-mips.hpkg`, `zlib_bootstrap_devel` and the
two GCC packages. `libz.so.1` needs only `libroot.so`.

Changes in this tree: `HaikuPortsCross/mips` (zlib 1.3.2 and the GCC package
versions), `CACHE_LINE_SIZE` 32 for mips in
`src/libs/compat/freebsd_network/compat/sys/param.h`, the compiler's own
headers in `gcc_syslibs_devel` (`syslibs.sh`), `OUTPUT` and `JAM_ARGS` in
`indy/jam.sh`.

Open: the other 20 stage 1 ports (logs in
`/mnt/europa/haiku-toolchain/build/port-NAME.log`). `gcc_bootstrap` (the
compiler that runs on Haiku) is not built. `binutils_bootstrap` 2.41 has no
mips n32 Haiku target (the patch of M1.2 is for 2.47).

## 2. Workspace and environment

| Path | Content |
|---|---|
| `/mnt/europa/haiku` | This fork. `origin` is haiku/haiku. |
| `/mnt/europa/iris` | SGI Indy emulator. Binaries: `target/release/iris`, `target/release/iris-ci`. |
| `/mnt/europa/os` | irix7 kernel for the same machine (Rust, GPL-3.0). `docs/research/indy-hardware.md` is a hardware reference with known errors (section 3.15). |
| `/mnt/europa/redox` | Redox fork that netboots on iris (MIT). `tools/ecoff`, `tools/indy-gate.sh`, `docs/adr/`. |
| `/mnt/europa/project` | Toolchain repository (same checkout as `/home/matt/projects/sgi-toolchain-new`). |
| `/home/matt/projects/sgi-toolchain-new/.scratch/rust-lang-gcc` | GCC 17.0.0 fork (`irix7/gcc`, based on rust-lang/gcc trunk). No Haiku support yet. |
| `/home/matt/projects/sgi-toolchain-new/.scratch/binutils-gdb-fork` | binutils 2.47 fork, branch `irix7/irix-binutils-2.47`. Has `mips*el-*-haiku*` only. |
| `/mnt/europa/toolchains/mips64-n32--uclibc--stable-2026.08-1` | Bootlin GCC 15.3. Runs on this host. Used for M0 only. |
| `/var/lib/sgi-netboot` | TFTP root for the physical Indy. Already holds `irix7` and `redox`. |
| `/mnt/europa/haiku-toolchain` | To be created in M1: toolchain worktrees, build trees, install prefix. |

Host facts:

- The host is NixOS. `jam`, `gcc` and `make` are not on `PATH`. Every build runs
  inside the flake devshell from task M0.1.
- `/` has 7.9 GB free. `/mnt/europa` is NFS with 6 TB free.
- The prebuilt prefix `.scratch/toolchain-17.0.0/prefix` does not run: its ELF
  interpreter path in `/nix/store` no longer exists. M1 builds a new prefix.
- Unix socket paths are limited to about 107 bytes. Keep gate work directories
  short, for example `/mnt/europa/haiku/generated.mips/gate`.
- Haiku needs its own `jam` (`Jamrules` rejects other variants). It is built
  from the `jam/` directory of github.com/haiku/buildtools.

Netboot server (dnsmasq on this host, `callisto`, 10.1.1.3):

- `tftp-root=/var/lib/sgi-netboot`, `dhcp-host=08:00:69:12:12:12,10.1.1.112`,
  `dhcp-boot=irix7,,10.1.1.3`.
- The config comes from the NixOS system configuration. Changing it is the
  owner's job. M8 must check whether the PROM requests the file named in
  `bootp()haiku` or the `dhcp-boot` name.

## 3. Machine reference

### 3.1 Identity and CPUs

The board is silkscreened IP24. The PROM, Linux and NetBSD call it IP22. It has
one CPU and no PCI. The system is big-endian.

| CPU | Where | I/D cache | Line | Ways | Notes |
|---|---|---|---|---|---|
| R4600 100 or 133 MHz | owner's Indy (target) | 16 KB / 16 KB | 32 B | 2 (8 KB per way) | 64-bit MIPS III. Count runs at half the core clock. No `rdhwr`. Cache errata in 3.16. |
| R4400 | iris default (`cpu = "r4400"`, PRId 0x0440) | 16 KB / 16 KB | 16 B | 1 | 1 MB L2 with 128-byte lines. Count fixed at 33 MHz. `rdhwr` raises Reserved Instruction. |
| R5000 | iris `cpu = "r5000"` (PRId 0x2321) | 32 KB / 32 KB | 32 B | 2 | Closest iris model to the R4600 cache geometry. |

All three have a 48-entry TLB where each entry maps an even/odd page pair, and
an FPU. The D-cache is write-back. Nothing keeps caches coherent with device
DMA. iris has no R4600 model, so R4600-only behaviour (3.16) is first exercised
on the machine itself.

### 3.2 Physical map

Reach devices through kseg1 (`0xA000_0000 + phys`) in the loader and in kernel
platform code. kseg0 is `0x8000_0000 + phys`, cached.

| Physical | Content |
|---|---|
| 0x0000_0000 to 0x0007_FFFF | Alias of the first 512 KiB of RAM. The exception vectors land here. |
| 0x0800_0000 | RAM. Indy: up to 256 MB. iris: four banks of 0, 8, 16, 32, 64 or 128 MB. |
| 0x1F0F_0000 | REX3 (Newport graphics) |
| 0x1FA0_0000 | Memory controller: SYSID +0x1C, MEMCFG0 +0xC4, MEMCFG1 +0xCC |
| 0x1FB8_0000 | HPC3: Ethernet RX engine +0x1_4000, TX engine +0x1_6000, INTSTAT +0x3_0000, INTSTAT1 +0x3_000C |
| 0x1FBC_4000 | WD33C93 SCSI: address register 0x1FBC_4003, data register 0x1FBC_4007 |
| 0x1FBD_4000 | Seeq 8003: register n at `0x1FBD_4000 + (n << 2) + 3` |
| 0x1FBD_9800 | IOC2: serial +0x30, keyboard/mouse +0x40, INT3 +0x80, 8254 +0xB0 |
| 0x1FBE_0000 | DS1386 RTC and NVRAM: register n at `0x1FBE_0000 + (n << 2) + 3` |
| 0x1FC0_0000 | PROM (32-bit ARCS, o32) |

Every 8-bit register file sits in byte lane 3 of a 32-bit word: `(reg << 2) + 3`.
This includes the Seeq.

RAM size comes from the memory controller, as `../redox` and `../os` do. Each of
MEMCFG0 and MEMCFG1 holds two 16-bit bank fields, high half first. For a field
`f`: valid if `f & 0x2000`; `shift = ((SYSID & 0x0f) >= 5) ? 24 : 22`;
`base = (f & 0xff) << shift`; `size = (((f >> 8) & 0x1f) + 1) << shift`. Checked
value: MEMCFG0 `0x6720_0000` is 32 MB at 0x0800_0000. Use only banks that extend
the contiguous range from 0x0800_0000.

### 3.3 PROM netboot contract

Verified on iris, which runs the SGI PROM image. Transcript of a passing run:

```
>> boot -f bootp()haiku
Setting $netaddr to 192.168.0.2 (from server iris)
Obtaining haiku from server iris
         64   +68   +16396 entry: 0x88040000
haiku: m0 alive
```

- The PROM loads ECOFF only. It prints text, data and bss sizes and the entry.
- Images are linked at kseg0 0x8804_0000 (physical 0x0804_0000). The first
  256 KiB of RAM stay reserved for the vectors and PROM data.
- At entry: kernel mode, interrupts off, caches on, `$a0` to `$a2` hold ARCS
  argc, argv and envp. `Status.BEV` is expected to be 1 (not measured).
- The image must set its own stack and must not return.
- The serial port is already set to 9600 8N1 by the PROM.
- TFTP uses 512-byte blocks. Keep the netboot image under 12 MB.
- `../redox` and `../os` never call ARCS after entry. This plan does not either
  in its first version (decision D10).

### 3.4 ECOFF layout

All fields big-endian. `B` is the load base, `T` the text size, `L` the number
of image bytes in the file, `Z` the bss size, `E` the entry.

| Offset | Size | Field | Value |
|---|---|---|---|
| 0 | 2 | f_magic | 0x0160 |
| 2 | 2 | f_nscns | 3 |
| 4 | 12 | f_timdat, f_symptr, f_nsyms | 0 |
| 16 | 2 | f_opthdr | 56 |
| 18 | 2 | f_flags | 0x100f |
| 20 | 2 | a.out magic | 0x0107 |
| 22 | 2 | vstamp | 0 |
| 24 | 4 | tsize | `T` |
| 28 | 4 | dsize | `L - T` |
| 32 | 4 | bsize | `Z` |
| 36 | 4 | entry | `E` |
| 40 | 4 | text_start | `B` |
| 44 | 4 | data_start | `B + T` |
| 48 | 4 | bss_start | `B + L` |
| 52 | 24 | gprmask, cprmask[4], gp_value | 0 |

Three 40-byte section headers follow at offsets 76, 116 and 156: 8-byte name,
then `s_paddr`, `s_vaddr`, `s_size`, `s_scnptr`, `s_relptr`, `s_lnnoptr` (4 bytes
each), `s_nreloc`, `s_nlnno` (2 bytes each), `s_flags` (4 bytes).

| Name | paddr = vaddr | size | scnptr | flags |
|---|---|---|---|---|
| `.text` | `B` | `T` | 208 | 0x20 |
| `.data` | `B + T` | `L - T` | `208 + T` | 0x40 |
| `.bss` | `B + L` | `Z` | 0 | 0x80 |

Image bytes start at file offset 208. Build the image from the ELF `PT_LOAD`
segments sorted by address, with gaps zero-filled. `T` is the end of the
executable segments. If that is 0 or covers the whole image, `T` is `L / 2`
rounded down to a multiple of 16. Reference implementation:
`../redox/tools/ecoff/src/main.rs`. Reference binary:
`../redox/tools/ecoff/target/release/ecoff convert IN OUT`.

### 3.5 Serial console

Z85C30 channel B is the PROM console and the rear port labelled "Serial Port 1".

| Register | kseg1 address |
|---|---|
| Channel B control | 0xBFBD_9833 |
| Channel B data | 0xBFBD_9837 |
| Channel A control | 0xBFBD_983B |
| Channel A data | 0xBFBD_983F |

Polled output that works without touching the PROM's setup: wait until bit 2 of
the control port reads 1 (transmit buffer empty), then write the byte to the data
port. Polled input: bit 0 of the control port means a byte is waiting in the
data port. `../os/kernel/src/console.rs` uses different addresses and is wrong
for hardware. Source: `../iris/src/dev/ioc.rs:48-51`, and the M0 boot.

### 3.6 Interrupts

Source: `../iris/src/dev/ioc.rs` and `../iris/docs/interrupt_map.md`. Registers
are bytes at the listed address plus 3.

| CPU line | Source |
|---|---|
| IP2 | INT3 local 0: `L0_STAT & L0_MASK` |
| IP3 | INT3 local 1: `L1_STAT & L1_MASK` |
| IP4, IP5 | 8254 counters 0 and 1. Do not use: the output is broken on the Indy. |
| IP6 | Bus error |
| IP7 | CP0 Count/Compare. Writing Compare acknowledges it. |

| Register | Physical |
|---|---|
| L0_STAT, L0_MASK | 0x1FBD_9880, 0x1FBD_9884 |
| L1_STAT, L1_MASK | 0x1FBD_9888, 0x1FBD_988C |
| MAP_STAT, MAP_MASK0, MAP_MASK1, MAP_POL | 0x1FBD_9890, 0x1FBD_9894, 0x1FBD_9898, 0x1FBD_989C |
| TMR_CLR, ERR_STAT | 0x1FBD_98A0, 0x1FBD_98A4 |
| 8254 counter 0, 1, 2, control | 0x1FBD_98B0, 0x1FBD_98B4, 0x1FBD_98B8, 0x1FBD_98BC (1 MHz clock) |

| Bit | Local 0 (IP2) | Local 1 (IP3) | Mapped (MAP_STAT) |
|---|---|---|---|
| 0 | REX3 FIFO full | Power button | Timer 0 |
| 1 | SCSI 0 | ISDN | Timer 1 |
| 2 | SCSI 1 | ISDN | |
| 3 | Ethernet | Cascade: `MAP_STAT & MAP_MASK1` | |
| 4 | MC DMA done | HPC3 DMA done | Keyboard and mouse |
| 5 | Parallel | AC fail | Serial |
| 6 | Graphics (GIO 1) | Vino | GIO expansion 0 |
| 7 | Cascade: `MAP_STAT & MAP_MASK0` | Vertical retrace (GIO 2) | GIO expansion 1 |

Ethernet acknowledge order, from `../os/docs/research/rx-interrupt.md`: read the
Seeq status registers, write CLRINT (0x02) to the HPC3 Ethernet reset register
(0x1FB9_5014), then write only the DMA bits (bits 5 and 6) back to HPC3 INTSTAT.
Writing the device bit (bit 4) while the pin is asserted causes a livelock at
about 225,000 interrupts per second.

### 3.7 Timers

- CP0 Count/Compare is the only usable interrupt timer. Under iris Count runs at
  33 MHz from the wall clock. On the R4600 it runs at half the core clock: 50 MHz
  on a 100 MHz module, 66.7 MHz on a 133 MHz module.
- The 8254 counts at 1 MHz. iris models it (`../iris/src/dev/pit8254.rs`), so
  calibrating Count against counter 2 can be tested on iris. Nothing in this
  workspace implements that calibration yet.
- The memory controller RPSS counter is a free-running 32-bit counter at
  0x1FA0_1004. Its divider register is at 0x1FA0_002C. iris models both
  (`../iris/src/dev/mc.rs`). `../os` and `../redox` do not use it. IRIX maps it
  into user programs as its fast clock. Task M9.3 uses it the same way.

### 3.8 Newport (REX3) display

Newport has no CPU-addressable frame buffer. Pixels reach the screen through
REX3 drawing commands or the HOSTRW data port. `../os/kernel/src/gfx.rs` is the
reference for the sequences below.

| Register | Offset from 0x1F0F_0000 |
|---|---|
| DRAWMODE1, DRAWMODE0 | 0x0000, 0x0004 |
| XYSTARTI, XYENDI | 0x0150, 0x0154 |
| WRMASK, COLORI | 0x0220, 0x0224 |
| HOSTRW0 | 0x0230 |
| DCBMODE, DCBDATA0 | 0x0238, 0x0240 |
| CONFIG, STATUS | 0x1330, 0x1338 |
| GO | add 0x0800 to a register offset to start the command |

- Coordinates are biased by 4096: `xy = ((x + 4096) << 16) | (y + 4096)`.
- STATUS: bit 3 graphics busy, bit 4 back end (DCB) busy, bits 12:7 FIFO level.
- Fill in 8-bit colour index mode: `DRAWMODE1 = 0x3000_7189`,
  `WRMASK = 0xFFFF_FFFF`, `COLORI = index`, XYENDI, XYSTARTI, then write
  `0x0306 | 0x20` to DRAWMODE0 + GO.
- Palette entry through the DCB (CMAP address 1): write
  `DCBMODE = (1 << 7) | (0 << 4) | 2 | 0x0084_8000 | (1 << 28) | (1 << 3)`, then
  `DCBDATA0 = index << 16`; then `DCBMODE = (1 << 7) | (2 << 4) | 3 | 0x0084_8000`,
  then `DCBDATA0 = rgb << 8`. Wait for STATUS bit 4 to clear before each entry.
- Host pixel upload uses HOSTRW0 with DRAWMODE0 bit 6 (colour from host). In the
  iris model 8-bit packed mode carries 4 pixels per 32-bit word, first pixel in
  bits 31:24. With DRAWMODE1 bit 10 (`rwdouble`) a 64-bit store to HOSTRW0
  carries 8 pixels (`../iris/src/dev/ng1/rex3_generic.rs`, `rex3.rs:419`). n32
  code can issue 64-bit stores from user mode. No code in this workspace uses
  HOSTRW yet. Task M6.1 establishes both sequences.
- With a graphics console the PROM programs video timing, XMAP and RAMDAC for
  1280 x 1024 in 8-bit colour index mode before it boots an image. With a serial
  console the PROM leaves the board uninitialised.
- The PROM leaves its own window offset, clip mode and screen masks in REX3.
  Nothing is drawn until they are reset: XYWIN (0x1324) = 0, CLIPMODE (0x1328)
  = `0xF << 9`, TOPSCAN (0x1320) = 0x3FF, SMASK0X/Y (0x0028, 0x002C) and SMASK1
  to SMASK4 (0x1300 to 0x131C) = 0. Found on iris on 2026-11-18. The sequence is
  `rex3_init()` in `indy/m0/hello.c`.
- Verified on iris with `indy/m0/hello.c`: DRAWMODE0 0x46 (draw, block, colour
  from host) uploads pixels. A write to HOSTRW0 + GO carries 4 pixels (32-bit
  store) or 8 pixels (64-bit store with `rwdouble`). Every row starts with a new
  word, and the last word of a row may be partly used.
- The owner's Indy has the 8-bit XL board: 8 bitplanes, no 24-bit mode. iris
  models the 24-bit board only. Both are in 8-bit colour index mode after the
  PROM, so all code here uses that mode and nothing else. Do not add RGB24
  paths.

### 3.9 Keyboard and mouse

An 8042-compatible controller: data port 0xBFBD_9843, status and command port
0xBFBD_9847. Status bit 0 is output full, bit 1 input full, bit 5 marks a mouse
byte. Its interrupt is mapped bit 4. The iris model sends scancode set 2 by
default, accepts keyboard command `0xF0 0x01` (select set 1) and ignores the
controller's translation bit. Translation behaviour is hardware unverified.

### 3.10 RTC and NVRAM

DS1386. Seconds 0x01, minutes 0x02, hours 0x04, date 0x08, month 0x09, year
0x0A, command 0x0B, all BCD. Clear command bit 7 (TE) to freeze the registers,
read, then set it again. The year register counts from 1940
(`../os/kernel/src/rtc.rs`, `../iris/src/dev/ds1x86.rs:151`). The Ethernet MAC
is in registers 314 to 319: byte addresses 0xBFBE_04EB, 0xBFBE_04EF and so on
(bus offset 0x4E8). All-ones means a dead battery.

### 3.11 Ethernet

Seeq 8003 with HPC3 DMA. Constants are in `../os/crates/irix7-seeq/src/lib.rs`:
registers 0 to 5 station address, 6 receive command and status, 7 transmit
command and status. Descriptors are `{pbuf, cntinfo, pnext}`, 8-byte aligned,
count in bits 13:0, EOX 0x8000_0000, EOP 0x4000_0000, XIE 0x2000_0000, OWN
0x0000_4000. A received buffer holds two pad bytes, the frame, then one status
byte (bit 5 set means good frame). Use a ring of 8 receive descriptors: a ring
of 16 broke under iris.

### 3.12 SCSI

WD33C93 with HPC3 DMA. `../os/kernel/src/scsi.rs` drives it with polled PIO. Its
DMA path has never run. Rules for DMA when it is written: end every chain with a
zero-length EOX descriptor, keep each descriptor within one 4 KiB physical
block, direction bit 2 of the HPC3 SCSI control register is 1 for memory to
device (hardware unverified; Linux's header comment says the opposite).

### 3.13 Cache instructions

| Operation | `cache` op |
|---|---|
| Hit_Writeback_Invalidate_D | 0x15 |
| Hit_Invalidate_D | 0x11 |
| Hit_Invalidate_I | 0x10 |
| Index_Writeback_Invalidate_D | 0x01 |
| Index_Invalidate_I | 0x00 |

Follow a group of cache operations with `sync`. Read line size and
associativity from the CP0 Config register at boot.

### 3.14 iris

- Machine profile `indy_ip24`. Config keys used here: `banks`, `no_audio`,
  `headless`, `nvram`, `[machine] profile` and `cpu`, `[network] mode` and
  `tftp_dir`, `[scsi.N] path`.
- `--headless` removes REX3. The PROM then uses the serial console.
- `--ci` without `--headless` renders REX3 offscreen. The PROM uses the graphics
  console and prints nothing on serial. `iris-ci screenshot FILE` works.
- The monitor TCP port accepts `ps2 type TEXT`, `ps2 enter`, `ps2 esc`,
  `screen wait QUIET_MS TIMEOUT_MS`, `ptr move X Y`, `ptr click X Y`.
- NAT network: guest 192.168.0.2, gateway 192.168.0.1 with BOOTP, TFTP and DNS.
  Traffic to the gateway reaches host loopback on the same port.
- A disk image attaches with `[scsi.1]` and `path = "FILE"`. The file must exist.
- `IRIS_NO_IDLE=1` stops idle parking. `IRIS_TFTP_TRACE=1` logs TFTP requests.

### 3.15 Known errors in `../os/docs/research/indy-hardware.md`

- It places the ARCS parameter block at physical 0x2000_1000. kseg1 0xA000_1000
  is physical 0x0000_1000.
- It says the Seeq uses byte lane 0. It uses lane 3.
- It describes IP2 as the HPC3 cascade. IP2 is INT3 local 0.
- Its REX3 register table has DRAWMODE0 and DRAWMODE1 swapped and other offsets
  wrong. Use section 3.8.
- Its year rule for the RTC (1900 or 2000) does not match the code. The base is
  1940.
- It puts the RPSS counter at MC offset 0x100C. iris decodes it at 0x1000 and
  0x1004.

### 3.16 R4600 specifics

Hardware unverified: iris cannot show any of this. Sources: Linux
`arch/mips/mm/c-r4k.c`, `arch/mips/include/asm/r4kcache.h`, the `WAR_R4600_*`
help texts in `arch/mips/Kconfig`, `arch/mips/mm/tlbex.c` and
`arch/mips/mm/sc-ip22.c`. Read them for facts.

- Revision: `PRId & 0xfffffff0` is 0x2010 for R4600 V1.x and 0x2020 for V2.0.
  Indy modules were built with V1.7 and with V2.0 chips.
- V1.x: a Hit-type D-cache operation (0x15, 0x11) gives wrong results unless
  the four instructions before it are free of loads and stores.
- V2.0: a Hit-type D-cache operation needs the D-cache refill buffer empty. One
  load from a kseg1 address empties it.
- Rule for every D-cache loop, on every CPU: interrupts off, one load from
  kseg1, four `nop`s, then the loop with no load or store inside it.
- V1.x: an Index-type I-cache operation can hit the wrong line when the `cache`
  instruction itself is fetched through the I-cache. Rule: invalidate ranges
  with Hit_Invalidate_I. Flush the whole I-cache once at boot from code that
  runs at its kseg1 address.
- Index operations select the second way with the address bit above the way
  size: bit 13 on the R4600.
- TLB write hazard: one `nop` before and one `nop` after `tlbwr` or `tlbwi`.
  After `mtc0` to Status, `tlbp` or `tlbr`, place three `nop`s before code that
  depends on the result.
- The R4600SC module carries a board-level secondary cache. It needs explicit
  enabling and explicit invalidation around DMA. Leave it untouched until task
  M9.6.
- The FPU raises Unimplemented Operation for denormal operands and results.

## 4. Decisions

**D1. ABI: `mips`, big-endian, n32, `-march=mips3 -mtune=r4600`, triplet
`mips-unknown-haiku`.** The R4600 is a 64-bit CPU, and n32 is the ABI SGI used
on it. Pointers and `long` are 32 bits. Registers are 64 bits. Reasons:

- Haiku's `bigtime_t` and `off_t` are 64-bit and fit in one register.
- Eight integer and eight floating-point argument registers suit Haiku's C++
  calls and app_server's floating-point drawing code. There are 32 FP registers.
- 64-bit atomics use `lld` and `scd`, with no kernel fallback.
- A syscall with up to eight arguments needs no copy from the user stack.
- User code can issue 64-bit stores, which halves the number of bus writes for a
  Newport upload (3.8).
- `../redox` and `../os` run n32 kernels on iris. The Redox kernel patch is
  MIT-licensed reference code for the assembly.
- The code runs on any MIPS III CPU, including the R4400 that iris models.

Consequences that every kernel task must respect:

- User threads run with `Status.UX` = 1. Kernel addressing stays 32-bit:
  `Status.KX` and `SX` are 0.
- Trap frames, context switches and `jmp_buf` hold 64-bit registers (`sd`,
  `ld`).
- A 32-bit value stored into a saved register is sign-extended first:
  `(int64)(int32)value`. Use one inline helper for every such store. A
  zero-extended value above 0x7FFF_FFFF breaks user code.
- User-mode TLB misses arrive at the XTLB vector, 0x8000_0080.
- `long double` is 64 bits on this target, as on Haiku's arm port. If the GCC 17
  MIPS backend cannot be overridden cleanly, keep the n32 default of 128 bits
  and add the `ldbl-128` headers to libroot as riscv64 does. Record the outcome
  in section 1.
- The PROM is o32. A firmware call needs a thunk that preserves the 64-bit
  callee-saved registers. This plan makes no firmware calls before M10.

**D2. Float.** Userland is hard-float. n32 uses 64-bit FP registers, so
`Status.FR` is 1 whenever the FPU is enabled. The kernel, kernel add-ons and
loader compile with `-msoft-float` and link the soft-float multilib of libgcc
and libsupc++. The kernel saves the 32 FP registers and FCSR lazily on
Coprocessor Unusable. New threads start with FCSR bit 24 (flush to zero) set,
which stops most denormal traps on the R4600. An Unimplemented Operation
exception delivers SIGFPE in the first version.

**D3. One CPU.** `SMP_MAX_CPUS` is 1. `arch_smp.cpp` follows
`src/system/kernel/arch/m68k/arch_smp.cpp`.

**D4. Page size 16 KB.** `PAGESIZE` is 16384 and `PAGE_SHIFT` is 14. The R4600's
cache ways are 8 KB, and the TLB's next page size above 4 KB is 16 KB. Each cache
way on the R4400, R4600 and R5000 is at most 16 KB, so a physical page has one
cache index at every virtual address. All cache maintenance can then use kseg0
addresses, which never miss in the TLB. With 4 KB pages the same page would land
in different cache sets through kseg0 and through a user mapping, and the kernel
would have to flush on every such access. 16 KB pages also give the 48-entry TLB
a reach of 1.5 MB. Costs: more memory per small area, and Haiku has no other
port with a page size above 4 KB. Known places to adjust are listed in 5.2.

**D5. Code models.**

| Code | Compile flags | Link |
|---|---|---|
| Userland | GCC default for the Haiku target: `-mabicalls -fPIC` | `-shared` for libraries and programs |
| Kernel | `-mabicalls -fno-pic -G 0 -msoft-float` | Dynamic executable (`ET_EXEC`) at a fixed kseg0 address, like x86 |
| Kernel add-ons | `-mabicalls -fPIC -G 0 -msoft-float` | `-shared` against `kernel.so` |
| Loader | `-mabicalls -fno-pic -G 0 -msoft-float` | Static, at 0x8804_0000 |

All four are n32. `kernel.so` is a copy of the kernel with its ELF type set to
`ET_DYN` (the `KernelSo` rule used by x86_64 and arm). Position-independent MIPS functions
need their own address in `$t9` on entry. GCC and ld handle this for C and C++
in every combination above. Hand-written assembly that calls a function through
a pointer, or calls into an add-on, must load the address into `$t9` and use
`jalr $t9`.

**D6. Address space.**

| Range | Use |
|---|---|
| 0x0010_0000 to 0x7FFF_FFFF | User. Follow the layout constants in `headers/private/kernel/arch/m68k/arch_kernel.h`. |
| 0x8000_0000 to 0x9FFF_FFFF (kseg0) | Kernel image at 0x8900_0000. Direct, cached access to all physical RAM: `virt = phys + 0x8000_0000`. Page tables are accessed here. |
| 0xA000_0000 to 0xBFFF_FFFF (kseg1) | Uncached device access for the loader and kernel platform code |
| 0xC000_0000 to 0xFFFF_FFFF (kseg2) | Kernel address space managed by the VM through page tables |

`KERNEL_BASE` is 0x8000_0000 and `KERNEL_SIZE` is 0x8000_0000. The translation
map answers `Query` for kseg0 addresses by arithmetic and treats `Map`, `Unmap`
and `Protect` there as no-ops. `arch_vm_init_post_area` blocks every part of
kseg0 and kseg1 outside the kernel image with `vm_block_address_range`, as
`X86PagingMethod64Bit::InitPostArea` does for its physical map. The physical
page mapper returns kseg0 addresses.

**D7. The loader builds the first kseg2 page tables.** Preloaded add-ons, the
kernel_args heap, and the boot stack are mapped in kseg2 from 0xC000_0000 by
page tables the loader creates. The kernel deletes some of those areas later,
and a freed kseg0 range could be handed out again by the VM. The loader reports
one virtual range from the kernel image start to the end of its kseg2
allocations, so early kernel allocations land in kseg2. The loader writes its
data through kseg0. Its only kseg2 access is the self-test of task M2.4.

**D8. Exception entry.** A per-CPU exception block lives at kseg0 0x8000_0400.
It holds the kernel stack pointer, the page directory pointer, the user thread
pointer, and a register save area. Handlers reach it with `lui $k1, 0x8000` and
a 16-bit offset, which needs no relocation. Name registers by number in
exception code (`$26` and `$27` are `$k0` and `$k1`), because n32 and o32 give
different names to `$8` through `$15`. The general exception handler saves all
registers into the block while `Status.EXL` is 1, clears EXL with interrupts
off, then copies the frame to the thread's kernel stack. Task M9.2 replaces the
copy with a wired TLB entry for the kernel stack. Kernel stacks are
mapped in kseg2, and a TLB miss taken with EXL set would enter the general
vector again and lose the first exception. The return path copies the frame back
to the block, sets EXL, restores from the block and runs `eret`. `eret` clears
the LL bit, so every return to user mode goes through it.

**D9. TLB and page tables.** Two levels. A page table is one 16 KB page of 4096
four-byte entries and maps 64 MB. An entry holds the EntryLo value in bits 31:2
and two software bits below: bit 0 "present", bit 1 "writable". The refill
handler shifts an entry right by 2. V and D double as the accessed and modified
flags: V is set when a page is mapped, D by the first write
(`MIPSVMTranslationMap::HandleFault`). Kernel entries have G set, also when
empty. The loader creates all 16 page tables of kseg2, so every address space
shares them. The page directory has 64
pointers. Empty directory slots point at one shared table of invalid entries, so
the refill handler needs no null check. PageMask is 0x6000 in every TLB write.
The same refill handler is installed at 0x8000_0000 (kernel-mode misses) and at
0x8000_0080 (user-mode misses). The copy at 0x8000_0080 first checks that bits
63:31 of BadVAddr are zero and takes the slow path otherwise. The handler ends
with `nop`, `tlbwr`, `nop`, the `$k1` load of D11, `eret`. Bring-up uses ASID 0
and flushes all entries on an address space switch. Task M9.1 adds ASIDs. An
unused TLB entry gets `EntryHi = 0x8000_0000 + index * 0x8000`, which can never
match. `Wired` stays 0 until M9.2.

**D10a. Root volume over the network (added 2026-11-18).** The default boot
volume is a RemoteDisk: a BFS image served by `remote_disk_server` (a host
tool of this tree, `jam remote_disk_server`) over UDP port 8765. The loader
passes `BOOT_METHOD_NET`, the station address from the NVRAM and the PROM's
`$netaddr` as the client IP. The kernel brings up `/dev/net/seeq8003/0` itself
(`vfs_net_boot.cpp`) and mounts the BFS volume of
`/dev/disk/virtual/remote_disk/0/raw`. The Indy's own disk is not touched.
The driver finds the server with a broadcast. `server=ADDRESS` on the PROM
command line, or the PROM variable `haikuserver`, names a server instead (the
loader hands the kernel a driver settings file `remote_disk`). The argument
`disk` on the PROM command line selects the SCSI disk path of D10. Verified on
iris: `BOOT_ARGS="server=192.168.0.1" ./indy/gate.sh
generated.mips/haiku-indy-netboot "Mounted boot partition"` with the server
running on the host. iris's NAT does not relay broadcasts, so the gate needs
`server=`. The protocol has no authentication: trusted networks only. A server
behind a router also needs a gateway, which the kernel's boot-time setup does
not configure yet.

**D10. Boot path.** The netboot file `haiku` is the ECOFF loader with the
netboot archive appended. The archive is a gzip tar holding the kernel and the
boot modules. The loader sizes RAM from MEMCFG, mounts the archive through a
`MemoryDisk` and tarfs, loads the kernel at its link address and jumps. It makes
no ARCS calls. It sets `BOOT_METHOD_HARD_DISK`, `BOOT_VOLUME_BOOTED_FROM_IMAGE`
and a disk identifier with all checksum offsets -1, so the kernel mounts the
first BFS volume it finds. That volume is a Haiku image on a SCSI disk. A
network root through RemoteDisk is M10 work. tarfs inflates at most 11 MB.

**D11. Thread pointer in `$k1`.** The MIPS ABIs reserve `$k0` and `$k1` for the
kernel, and compilers never allocate them. Every return to user mode, including the TLB refill handler, loads the
current thread's TLS base into `$k1` as its last step. libroot reads `$k1`. The
Reserved Instruction handler also emulates `rdhwr rt, $29`
(`(insn & 0xffe0ffff) == 0x7c00e83b`) by storing the same value in `rt`.

**D12. Time.** The scheduler timer is Count/Compare. The kernel measures the
Count rate at boot against 8254 counter 2 (1 MHz), which gives 33 MHz on iris
and 50 or 66.7 MHz on an R4600. `system_time()` in the kernel extends Count to
64 bits and converts with a fixed-point factor. The hardware timer is always
armed within 30 seconds so no Count wrap is missed. Userland `system_time()`
calls `_kern_system_time` until task M9.3 gives it the RPSS counter.

**D13. Interrupt vectors.** `install_io_interrupt_handler` numbers: 32 to 39 are
local 0 bits 0 to 7, 40 to 47 local 1, 48 to 55 mapped bits. Vectors 1 and 12
both mean mapped bit 4, because the ps2 bus manager installs handlers on those
two numbers.

**D14. Display.** A `newport` kernel driver and accelerant. The accelerant
reports one mode, 1280 x 1024 `B_CMAP8`, and returns a RAM shadow buffer as the
frame buffer. app_server already converts its back buffer to `B_CMAP8`
(`HWInterface::_CopyToFront`) and sets the palette through
`B_SET_INDEXED_COLORS`. app_server gains one optional accelerant hook that
receives each rectangle written to the front buffer. The accelerant pushes that
rectangle from the shadow to REX3. It uses 64-bit host writes (8 pixels per
store) when a readback test at start confirms them, and 32-bit writes
otherwise. The first version requires a display the PROM initialised (graphics
console). The generic `framebuffer` driver, the boot
splash and the kernel frame buffer console are not used. app_server no longer
calls 2D acceleration hooks, so REX3 fills and blits cannot speed it up.

**D15. Input.** The existing `ps2` bus manager and input_server add-ons are
reused. A new `isa` architecture backend maps port 0x60 to 0xBFBD_9843 and port
0x64 to 0xBFBD_9847. On mips the ps2 code leaves 8042 translation off and
selects scancode set 1 at the keyboard.

**D16. Toolchain.** GCC 17 from the owner's fork and binutils 2.47 from the
owner's fork, each on a new branch `haiku-mips`. The Haiku target layer is
ported from the HaikuPorts patch set `gcc-15.3.0_2026_10_11.patchset`. GCC is
configured with `--with-abi=n32 --with-arch=mips3 --with-tune=r4600` and gains a
soft-float multilib. The Haiku build pins `-std=gnu17` for C and `-std=gnu++17`
for C++ on mips, because newer GCC defaults differ from the GCC 13 defaults
upstream builds with.

**D17. Third-party packages.** Kernel and loader builds need libgcc, libsupc++,
the C++ headers and the zlib sources. Upstream takes them from HaikuPorts
packages, which do not exist for mips. M1 defines those build features from a
local seed directory. M5 builds the userland packages with haikuporter and
`haikuports.cross`, as `docs/develop/packages/Bootstrapping.rst` describes.

## 5. File changes

This section lists every file the port deletes, changes or adds. Line numbers
refer to commit 8bf39ccf93. The milestone that makes each change is in
brackets.

### 5.1 Files to delete [M1.1]

42 placeholder files from the abandoned little-endian port. None of their
content is correct for this port.

```
src/system/kernel/arch/mips/          arch_asm.S arch_cpu.cpp arch_dbg_console.cpp
                                      arch_debug.cpp arch_faults.cpp arch_int.cpp
                                      arch_pmap.cpp arch_smp.cpp arch_thread.cpp
                                      arch_timer.cpp arch_vm.cpp
headers/private/kernel/arch/mipsel/   arch_cpu.h arch_debug.h arch_int.h
                                      arch_kernel_args.h arch_kernel.h arch_mmu.h
                                      arch_platform.h arch_system_info.h
                                      arch_thread.h arch_thread_types.h
                                      arch_user_debugger.h arch_vm.h
                                      arch_vm_translation_map.h arch_vm_types.h
                                      ktypes.h types.h
headers/private/system/arch/mipsel/   arch_commpage_defs.h arch_config.h
                                      arch_cpu_defs.h arch_elf.h
                                      arch_real_time_data.h asm_defs.h
headers/os/arch/mipsel/               arch_debugger.h
headers/posix/arch/mipsel/            arch_setjmp.h limits.h signal.h
src/kits/debug/arch/mipsel/           arch_debug_support.cpp
src/tools/gensyscalls/arch/mipsel/    arch_gensyscalls.h
headers/private/kernel/boot/platform/routerboard_mipsel/
                                      platform_arch.h platform_kernel_args.h
                                      platform_stage2_args.h
```

### 5.2 Existing files to change

**`configure`** [M1.5]

```
line 34 (help text):       add "mips" after "m68k",
after `m68k-*)` (290):     		mips-*)		targetCpu=mips;;
after `m68k` (705):        	mips
after `m68k)` (760):       				mips)		targetMachine=mips-unknown-haiku;;
```

**`build/jam/ArchitectureRules`** [M1.5]

```
# ArchitectureSetup, in the archFlags switch after the riscv64 case (line 43):
		case mips : archFlags += -march=mips3 -mtune=r4600 -mabi=n32 ;

# ArchitectureSetup, after "HAIKU_LINKFLAGS_$(architecture) += $(ccBaseFlags) ;":
	if $(cpu) = mips {
		# GCC 17 defaults to gnu23 and gnu++20. Upstream builds with the
		# GCC 13 defaults.
		HAIKU_CCFLAGS_$(architecture) += -std=gnu17 ;
		HAIKU_C++FLAGS_$(architecture) += -std=gnu++17 ;
	}

# KernelArchitectureSetup, first switch, before "case x86 :":
		case mips :
			HAIKU_KERNEL_PLATFORM ?= arcs ;
			HAIKU_BOOT_TARGETS += arcs ;

			HAIKU_BOOT_FLOPPY_IMAGE_SIZE = 1440 ;
			HAIKU_BOOT_ARCHIVE_IMAGE_OFFSET = 192 ; # in kB - unused

# KernelArchitectureSetup, second switch, before "case ppc :":
		case mips :
			# The kernel is non-PIC at a fixed kseg0 address. Add-ons are PIC.
			HAIKU_KERNEL_PIC_CCFLAGS = -fno-pic ;
			HAIKU_KERNEL_CCFLAGS += -msoft-float -G0 ;
			HAIKU_KERNEL_C++FLAGS += -msoft-float -G0 ;

# KernelArchitectureSetup, boot target switch, before "case * :":
			case arcs :
				HAIKU_BOOT_$(bootTarget:U)_CCFLAGS += -fno-pic -msoft-float -G0
					-Wno-error=main ;
				HAIKU_BOOT_$(bootTarget:U)_C++FLAGS += -fno-pic -msoft-float -G0
					-Wno-error=main ;

# ArchitectureSetupWarnings, after the ppc case:
		case mips :
			return ;
```

**`build/jam/BuildFeatures`** [M1.6]. In the `else` branches of the
`gcc_syslibs_devel` block (line 93) and the `zlib` block (line 405): when
`TARGET_PACKAGING_ARCH` is mips and the seed directory exists, set the same
attribute names with `SetBuildFeatureAttribute` and call
`EnableBuildFeatures`.

**Image and package lists** [M4.4, M6.5, M7.1]

| File | Change |
|---|---|
| `build/jam/images/definitions/minimum` | Line 368 `isa@x86,x86_64,mips`. Line 372 `ps2@x86,x86_64,mips`. Add `indy@mips` to `SYSTEM_ADD_ONS_BUS_MANAGERS`. Add `newport@mips` to the graphics drivers, `newport.accelerant@mips` to the accelerants, `seeq8003@mips` to the network drivers. |
| `build/jam/packages/Haiku` and `HaikuBootstrap` | `isa@x86,x86_64,mips` (lines 338 and 318). Add `wd33c93_hpc3@mips` to the SCSI busses list next to `ahci`. |
| `build/jam/images/NetBootArchive` | Add `indy@mips` to the bus managers (line 33 list) and `wd33c93_hpc3@mips` to the busses. At the end, a target that runs `elf2ecoff.py boot_loader_arcs haiku --append haiku-netboot.tgz` and fails above 12 MB. |

**Headers**

```
headers/config/HaikuConfig.h [M1.1], replace the __MIPSEL__ block (lines 46-49):
	#elif defined(__mips__) && defined(__MIPSEB__)
	#	define __HAIKU_ARCH					mips
	#	define __HAIKU_ARCH_ABI				"mips"
	#	define __HAIKU_ARCH_MIPS			1
	#	define __HAIKU_BIG_ENDIAN			1

headers/posix/fenv.h [M1.1], before the #else:
	#elif defined(__mips__)
	#  include <arch/mips/fenv.h>

headers/os/kernel/debugger.h [M1.1]:
	#include <arch/mipsel/arch_debugger.h>   ->  #include <arch/mips/arch_debugger.h>
	#elif defined(__MIPSEL__)                ->  #elif defined(__mips__)
	mipsel_debug_cpu_state                   ->  mips_debug_cpu_state

headers/os/package/PackageArchitecture.h [M1.5], after RISCV64:
	B_PACKAGE_ARCHITECTURE_MIPS	= 11,

headers/private/kernel/kernel.h [M1.5], line 70:
	#if defined(__mips__)
		#define KERNEL_STACK_SIZE		(B_PAGE_SIZE * 1)	// 16 kB
	#elif defined(B_HAIKU_64_BIT)

headers/private/kernel/arch/elf.h [M4.1], prototype under #ifdef __mips__:
	extern int arch_elf_relocate_got(struct elf_image_info* image,
		struct elf_image_info* resolveImage);
```

**Sources and Jamfiles**

```
src/kits/package/PackageInfo.cpp [M1.5], kArchitectureNames:
	"riscv64",
	"mips"

src/system/kernel/Jamfile [M3.1], line 128:
	if $(HAIKU_ARCH) in x86_64 arm mips {

src/system/kernel/elf.cpp [M4.1], in elf_relocate() before "deal with the rels":
	#ifdef __mips__
		status = arch_elf_relocate_got(image, resolveImage);
		if (status < B_OK)
			return status;
	#endif

src/system/kernel/debug/core_dump.cpp [M3.1], line 1130:
	__HAIKU_ARCH_MIPSEL  ->  __HAIKU_ARCH_MIPS

src/system/runtime_loader/runtime_loader.cpp [M5.3], after the EM_RISCV case:
		case EM_MIPS:
			architecture = "mips";
			break;

src/system/boot/Jamfile [M2.1], after "actions BuildBiosLoader":
	rule BuildEcoffLoader {
		local haikuLoader = $(1) ;
		local bootLoader = $(2) ;

		Depends $(haikuLoader) : $(bootLoader) ;
		MakeLocateDebug $(haikuLoader) ;
	}

	actions BuildEcoffLoader {
		$(RM) -f $(1)
		python3 $(HAIKU_TOP)/build/scripts/elf2ecoff.py $(2) $(1)
	}
and in the loader switch before "case * :" (line 465):
			case arcs :
				BuildEcoffLoader haiku_loader.$(TARGET_BOOT_PLATFORM) : <revisioned>boot_loader_$(TARGET_BOOT_PLATFORM) ;

src/system/boot/loader/Jamfile [M2.1], after the "m68k" case (line 86):
				case "mips" :
				{
					DEFINES += BOOT_SUPPORT_ELF32 ;
				}

src/system/libroot/os/syscalls.S [M5.2], before the #else:
	#elif defined(ARCH_mips)
	#	include "arch/mips/syscalls.inc"

src/system/libroot/posix/malloc/openbsd/wrapper.c [M5.2], after line 23:
	#elif B_PAGE_SIZE == 16384
	#define _MAX_PAGE_SHIFT 14

src/add-ons/kernel/bus_managers/Jamfile [M4.2]:
	SubInclude HAIKU_TOP src add-ons kernel bus_managers indy ;
src/add-ons/kernel/busses/scsi/Jamfile [M4.3]:
	SubInclude HAIKU_TOP src add-ons kernel busses scsi wd33c93_hpc3 ;
src/add-ons/kernel/drivers/network/ether/Jamfile [M7.1]:
	HaikuSubInclude seeq8003 ;
src/add-ons/kernel/drivers/graphics/Jamfile [M6.2]:
	SubInclude HAIKU_TOP src add-ons kernel drivers graphics newport ;
src/add-ons/accelerants/Jamfile [M6.3]:
	SubInclude HAIKU_TOP src add-ons accelerants newport ;

src/add-ons/kernel/bus_managers/ps2/ps2_common.cpp [M6.5],
in ps2_setup_command_byte() after "cmdbyte |= PS2_BITS_TRANSLATE_SCANCODES;":
	#ifdef __mips__
		cmdbyte &= ~PS2_BITS_TRANSLATE_SCANCODES;
	#endif
src/add-ons/kernel/bus_managers/ps2/ps2_keyboard.cpp [M6.5], in the probe after
the keyboard answers: under #ifdef __mips__ send 0xF0 then 0x01.

src/servers/app/drawing/HWInterface.h and .cpp [M6.4]:
	virtual void _FrontBufferUpdated(int32 left, int32 top, int32 right,
		int32 bottom);     // empty in the base class
	called as the last statement of HWInterface::_CopyToFront().
src/servers/app/drawing/interface/local/AccelerantHWInterface.h and .cpp [M6.4]:
	look up the hook B_FRONT_BUFFER_UPDATED with the other optional hooks
	(line 371 onward) and override _FrontBufferUpdated() to call it.
```

### 5.3 New files in this repository

Build and port support:

| File | Content | Task |
|---|---|---|
| `flake.nix`, `flake.lock` | Devshell `haiku-indy-dev` with Haiku's jam | M0.1 |
| `build/scripts/elf2ecoff.py` | ELF32 to ECOFF, `--append` | M0.2 |
| `indy/m0/Makefile`, `start.S`, `hello.c`, `link.ld` | Freestanding test image | M0.3, M6.1 |
| `indy/gate.sh`, `indy/gate-gfx.sh`, `indy/Makefile` | Gates and their targets | M0.4 |
| `indy/toolchain/build.sh`, `seed.sh` | Cross toolchain and seed directory | M1.4, M1.6 |
| `indy/toolchain/syslibs.sh` | Packages `gcc_syslibs` and `gcc_syslibs_devel` | M5.4 |
| `build/jam/repositories/HaikuPorts/mips`, `HaikuPortsCross/mips` | Package lists, modelled on riscv64's | M1.5, M5.5 |
| `src/tools/makebootable/platform/arcs/Jamfile` | Copy of `platform/u-boot/Jamfile` with the directory name changed | M4.4 |
| `src/bin/makebootable/platform/arcs/Jamfile`, `makebootable.cpp` | Copies of the `u-boot` ones (a program that does nothing) | M4.4 |

Public and private headers:

| File | Content | Task |
|---|---|---|
| `headers/os/arch/mips/arch_debugger.h` | `struct mips_debug_cpu_state`: 32 registers, pc, hi, lo as `uint64`, 32 doubles, fcsr | M1.1 |
| `headers/posix/arch/mips/limits.h` | `PAGESIZE 16384` | M1.1 |
| `headers/posix/arch/mips/signal.h` | `struct vregs` | M1.1 |
| `headers/posix/arch/mips/arch_setjmp.h` | `__jmp_buf` | M1.1 |
| `headers/posix/arch/mips/fenv.h` | FCSR flag, mask and rounding constants | M1.1 |
| `headers/private/system/arch/mips/arch_config.h` | As riscv64's | M3.1 |
| `headers/private/system/arch/mips/arch_commpage_defs.h` | Signal handler and thread exit entries | M3.1 |
| `headers/private/system/arch/mips/arch_cpu_defs.h` | CP0 register numbers, Status and Cause bits, `SPINLOCK_PAUSE` | M3.1 |
| `headers/private/system/arch/mips/arch_elf.h` | `R_MIPS_*`, `DT_MIPS_*`, `EF_MIPS_*` | M2.6 |
| `headers/private/system/arch/mips/arch_real_time_data.h` | Fields for the user-mode clock | M3.1, M9.3 |
| `headers/private/system/arch/mips/asm_defs.h` | `FUNCTION`, `FUNCTION_END`, `SYMBOL` macros | M3.1 |
| `headers/private/kernel/arch/mips/arch_atomic.h` | `ll`/`sc` and `lld`/`scd` atomics | M3.1 |
| `headers/private/kernel/arch/mips/arch_cpu.h` | `arch_cpu_info`, CP0 accessors, cache routine prototypes | M3.1 |
| `headers/private/kernel/arch/mips/arch_debug.h` | `struct arch_debug_registers` | M3.1 |
| `headers/private/kernel/arch/mips/arch_int.h` | Interrupt enable and disable inlines, `NUM_IO_VECTORS 64` | M3.1 |
| `headers/private/kernel/arch/mips/arch_kernel.h` | Address space constants of D6 | M3.1 |
| `headers/private/kernel/arch/mips/arch_kernel_args.h` | Page directory address, RAM top | M3.1 |
| `headers/private/kernel/arch/mips/arch_system_info.h` | Empty, as riscv64's | M3.1 |
| `headers/private/kernel/arch/mips/arch_thread.h` | Thread inlines | M3.1 |
| `headers/private/kernel/arch/mips/arch_thread_types.h` | `struct iframe`, `arch_thread`, `arch_team`, `arch_fork_arg` | M3.2 |
| `headers/private/kernel/arch/mips/arch_user_debugger.h` | Empty debug info structs | M3.1 |
| `headers/private/kernel/arch/mips/arch_vm.h` | `PAGE_SHIFT 14` | M3.1 |
| `headers/private/kernel/arch/mips/arch_vm_translation_map.h` | kseg0 conversion inlines | M3.3 |
| `headers/private/kernel/arch/mips/arch_vm_types.h` | Include guard only | M3.1 |
| `headers/private/kernel/arch/mips/arch_exception_block.h` | Layout of the block at 0x8000_0400, usable from C and assembly | M3.2 |
| `headers/private/kernel/arch/mips/arch_indy.h` | Device addresses and bits of section 3 | M3.1 |
| `headers/private/kernel/boot/arch/mips/arch_cpu.h` | Loader CPU declarations | M2.1 |
| `headers/private/kernel/boot/platform/arcs/platform_kernel_args.h` | `SMP_MAX_CPUS 1`, range limits, empty `platform_kernel_args` | M2.1 |
| `headers/private/kernel/boot/platform/arcs/platform_stage2_args.h` | `struct platform_stage2_args` | M2.1 |
| `headers/private/graphics/newport/newport_info.h` | Shared info struct and ioctl codes of the driver and accelerant | M6.2 |
| `headers/private/graphics/common/front_buffer_hook.h` | `B_FRONT_BUFFER_UPDATED` (a value at or above `B_ACCELERANT_PRIVATE_START`) and its function type | M6.4 |

Loader:

| File | Content | Task |
|---|---|---|
| `src/system/ldscripts/mips/boot_loader_arcs.ld` | Link at 0x88040000 | M2.1 |
| `src/system/boot/arch/mips/Jamfile`, `arch_cpu.cpp` | `boot_arch_mips.o`, compiles the kernel's `arch_elf.cpp` in boot mode | M2.1, M2.6 |
| `src/system/boot/platform/arcs/Jamfile` | `boot_platform_arcs.o` | M2.1 |
| `src/system/boot/platform/arcs/crt0.S` | Entry: stack, bss, call `_start` | M2.1 |
| `src/system/boot/platform/arcs/start.cpp` | `_start`, `platform_start_kernel`, `platform_exit`, `platform_boot_options` | M2.1, M2.5 |
| `src/system/boot/platform/arcs/entry.S` | Vector install, final jump | M2.5 |
| `src/system/boot/platform/arcs/debug.cpp` | `dprintf`, `panic`, debug log buffer | M2.1 |
| `src/system/boot/platform/arcs/console.cpp`, `console.h` | Serial text console | M2.1 |
| `src/system/boot/platform/arcs/cpu.cpp` | Cache flush, CP0 setup | M2.5 |
| `src/system/boot/platform/arcs/mmu.cpp`, `mmu.h` | MEMCFG sizing, allocator, kseg2 page tables | M2.2, M2.4 |
| `src/system/boot/platform/arcs/devices.cpp` | `MemoryDisk` boot device over the archive | M2.3 |
| `src/system/boot/platform/arcs/menu.cpp`, `video.cpp` | Menu hooks, video stubs that leave `frame_buffer.enabled` false | M2.1 |

Kernel:

| File | Content | Task |
|---|---|---|
| `src/system/ldscripts/mips/kernel.ld` | Link at 0x89000000 | M3.1 |
| `src/system/kernel/platform/arcs/Jamfile`, `platform.cpp` | `kernel_platform_arcs.o` | M3.1 |
| `src/system/kernel/lib/arch/mips/Jamfile` | `kernel_os_arch_mips.o`, `kernel_lib_posix_arch_mips.o` | M3.1 |
| `src/add-ons/kernel/cpu/mips/Jamfile` | Two lines, as riscv64's | M3.1 |
| `src/system/kernel/arch/mips/Jamfile` | `kernel_arch_mips.o` | M3.1 |
| `.../arch_exceptions.S` | Vector templates, general handler, TLB refill | M3.2 |
| `.../arch_asm.S` | Context switch, interrupt state, `arch_debug_call_with_fault_handler` | M3.4 |
| `.../arch_tlb.S` | TLB write, probe, flush | M3.3 |
| `.../arch_fpu.S` | FPU save and restore | M5.1 |
| `.../asm_offsets.cpp` | Struct offsets for assembly | M3.2 |
| `.../arch_int.cpp` | Vector install, exception dispatch, INT3 interrupt controller | M3.2, M3.6 |
| `.../arch_vm.cpp`, `arch_vm_translation_map.cpp` | `arch_vm_*` functions, physical page mapper over kseg0 | M3.3 |
| `.../MIPSVMTranslationMap.cpp`, `MIPSVMTranslationMap.h` | Two-level page tables | M3.3 |
| `.../arch_thread.cpp` | Threads, user entry, signal frames, fork frames | M3.4, M5.1 |
| `.../arch_timer.cpp` | Count/Compare, 8254 calibration, `system_time()` | M3.5 |
| `.../arch_cpu.cpp` | CPU identification, cache routines, TLB invalidation | M3.7 |
| `.../arch_debug_console.cpp` | Serial console of section 3.5 | M3.1 |
| `.../arch_debug.cpp` | Stack traces, debug variables | M3.7 |
| `.../arch_platform.cpp` | Platform init | M3.1 |
| `.../arch_real_time_clock.cpp` | DS1386 | M3.7 |
| `.../arch_smp.cpp`, `arch_system_info.cpp`, `arch_user_debugger.cpp` | As m68k's and riscv64's | M3.7 |
| `.../arch_commpage.cpp` | Signal trampoline and thread exit stubs | M5.1 |
| `.../arch_elf.cpp` | Loader half (M2.6), GOT and `R_MIPS_REL32` for add-ons (M4.1) | M2.6, M4.1 |

Userland:

| File | Content | Task |
|---|---|---|
| `src/system/glue/arch/mips/Jamfile`, `crti.S`, `crtn.S` | `_init` and `_fini` prologue and epilogue | M5.2 |
| `src/system/libroot/os/arch/mips/Jamfile` | Lists the files below plus `generic_atomic.cpp`, `generic_stack_trace.cpp`, `generic_system_time_nsecs.cpp` | M5.2 |
| `.../os/arch/mips/byteorder.S`, `stack_frame.S`, `syscalls.inc`, `system_time.cpp`, `thread.cpp`, `time.cpp`, `tls.cpp`, `cachectl.cpp` | As riscv64's, with the n32 rules of M5.2 | M5.2 |
| `src/system/libroot/posix/arch/mips/Jamfile`, `fenv.c`, `sigsetjmp.S`, `siglongjmp.S`, `setjmp_internal.h` | setjmp and fenv | M5.2 |
| `src/system/libroot/posix/string/arch/mips/Jamfile` | Generic `memcpy` and `memset` | M5.2 |
| `src/system/libroot/posix/glibc/arch/mips/Jamfile` | Generic sources only | M5.2 |
| `src/system/libroot/posix/glibc/include/arch/mips/config.h`, `bits/wordsize.h` | As arm's (`__WORDSIZE` 32, 64-bit `long double`) | M5.2 |
| `src/system/libroot/posix/musl/arch/mips/atomic_arch.h` | `ll`/`sc` compare and swap | M5.2 |
| `src/system/libroot/posix/musl/math/mips/Jamfile` | Generic sources only, as m68k's | M5.2 |
| `src/system/runtime_loader/arch/mips/Jamfile`, `arch_relocate.cpp` | MIPS dynamic relocation | M5.3 |
| `src/tools/gensyscalls/arch/mips/arch_gensyscalls.h` | `long long` alignment types | M3.1 |
| `src/kits/debug/arch/mips/arch_debug_support.cpp` | Reads pc and frame pointer from the CPU state | M5.2 |

Drivers:

| File | Content | Task |
|---|---|---|
| `src/add-ons/kernel/bus_managers/indy/Jamfile`, `indy.cpp` | Root bus that registers the fixed devices | M4.2 |
| `src/add-ons/kernel/busses/scsi/wd33c93_hpc3/Jamfile`, `wd33c93.cpp`, `wd33c93.h`, `hpc3_dma.cpp` | SCSI controller | M4.3, M4.5 |
| `src/add-ons/kernel/bus_managers/isa/arch/mips/Jamfile`, `isa_controller.c`, `isa_dma.c` | Ports 0x60 and 0x64 mapped to the 8042 registers | M6.5 |
| `src/add-ons/kernel/drivers/graphics/newport/Jamfile`, `driver.cpp`, `device.cpp`, `newport.cpp`, `driver.h` | Graphics driver | M6.2 |
| `src/add-ons/accelerants/newport/Jamfile`, `accelerant.cpp`, `accelerant.h`, `hooks.cpp`, `mode.cpp`, `upload.cpp` | Accelerant with the upload hook | M6.3 |
| `src/add-ons/accelerants/newport/cursor.cpp` | VC2 hardware cursor | M9.4 |
| `src/add-ons/kernel/drivers/network/ether/seeq8003/Jamfile`, `driver.cpp`, `seeq8003.cpp`, `seeq8003.h` | Ethernet driver | M7.1 |

### 5.4 Changes outside this repository

On branch `haiku-mips` of each fork, in worktrees under
`/mnt/europa/haiku-toolchain/src` (section 6):

| Repository | Files |
|---|---|
| binutils 2.47 fork [M1.2] | `bfd/config.bfd`, `ld/configure.tgt`, `ld/emulparams/elf_mips_haiku.sh` (new), `ld/Makefile.am`, `ld/Makefile.in` |
| GCC 17 fork [M1.3] | Every file of the HaikuPorts 15.3 patch set except libsanitizer. The core ones: `gcc/config.gcc`, `gcc/config/haiku.h`, `haiku.opt`, `haiku-protos.h`, `haiku-stdint.h`, `t-haiku`, the Haiku driver source the patch set adds under `gcc/config/`, `libgcc/config.host`, `libgcc/config/t-haiku`, `libstdc++-v3/configure.host`, `crossconfig.m4`, `config/os/haiku/*` and the regenerated `configure` scripts. New for MIPS: `gcc/config/mips/haiku.h`, `gcc/config/mips/t-haiku`. |
| haikuporter [M5.5] | The architecture table: add `mips` with triplet `mips-unknown-haiku` |
| haikuports.cross [M5.5] | `ARCHITECTURES` in each recipe of the stage 1 list |

No change is needed in `../iris`, `../os` or `../redox`.

### 5.5 Checking for missed sites

Every site in 5.2 was found with these commands. Run them again after a rebase
onto a newer upstream commit.

```
grep -n "riscv" configure
grep -rn "riscv64" build/jam build/scripts
grep -rn "riscv64" --include=Jamfile src | grep -v "^src/libs/"
grep -rn "__riscv\|EM_RISCV\|__HAIKU_ARCH_RISCV64" headers src/system src/kits \
	| grep -v "/arch/"
```

## 6. Toolchain and seed directory

Layout under `/mnt/europa/haiku-toolchain`:

```
src/gcc        git worktree of the GCC 17 fork, branch haiku-mips
src/binutils   git worktree of the binutils 2.47 fork, branch haiku-mips
build/         object trees
cross-tools-mips/   install prefix: bin/mips-unknown-haiku-gcc and friends
seed/          libgcc, libsupc++, C++ headers, zlib sources for the Haiku build
```

Create each worktree with `git -C <fork> worktree add <path> -b haiku-mips`. Do
not commit to the forks' existing branches.

binutils additions for `mips*-*-haiku*` (big-endian, n32): a `bfd/config.bfd`
stanza with `targ_defvec=mips_elf32_ntrad_be_vec` and the selectable vectors of
the `mips64*-*-linux*` stanza, an `ld/configure.tgt` stanza with
`targ_emul=elf_mips_haiku`, and `ld/emulparams/elf_mips_haiku.sh` that sources
`elf32btsmipn32.sh` and `elf_haiku.sh`. Register the emulation in
`ld/Makefile.am` and `ld/Makefile.in` next to `eelf_mipsel_haiku.c`. Add
`xelf_mips_haiku) ELFSIZE=32 ;;` to the name check in
`ld/emulparams/elf32bmipn32-defs.sh`: it accepts only names that match
`elf32*n32*`. All of this is in
`indy/toolchain/patches/binutils-haiku-mips.patch`. gas already
matches `mips-*-haiku*`; GCC passes the ABI to it on every call. Linker scripts
for this port use `OUTPUT_FORMAT("elf32-ntradbigmips")`.

GCC additions: the Haiku layer, then the MIPS parts. The 13.3 patch set touches
81 files (`grep "^+++ b/" <patchset>` lists them); apply the 15.3 set named in
D16. Haiku's 13.3 tree has a little-endian stanza to start from:

```
mipsel-*-haiku*)
	target_cpu_default="MASK_ABICALLS"
	tm_file="elfos.h ${tm_file} haiku.h mips/haiku.h haiku-stdint.h"
	tmake_file="${tmake_file} mips/t-elf t-haiku"
```

Generalise it to `mips*-*-haiku*`. Make n32 and MIPS III the defaults the way
the fork's own `mips-sgi-irix6*` stanza does for a `mips-` triplet
(`MIPS_ABI_DEFAULT=ABI_N32`, `MIPS_ISA_DEFAULT=3`). In
`gcc/config/mips/haiku.h` define `__HAIKU__`, drop the hard-coded `__MIPSEL__`
defines (the MIPS backend defines the endianness macros), set `long double` to
64 bits (D1), and set `LINK_SPEC` to
`-m elf_mips_haiku %{!r:-shared} %{nostart:-e 0}`. Add a `t-haiku` fragment for
mips with `MULTILIB_OPTIONS = msoft-float` and `MULTILIB_DIRNAMES = soft-float`.
Add `mips*-*-haiku*` to `libgcc/config.host`.

Configure both with `--target=mips-unknown-haiku
--prefix=/mnt/europa/haiku-toolchain/cross-tools-mips`. Take every other option
from `build/scripts/build_cross_tools_gcc4` (read it: it stages
`headers/config`, `headers/os` and `headers/posix` into a sysroot first), and add
`--with-arch=mips3 --with-tune=r4600 --with-abi=n32 --with-float=hard` to GCC. The fork has no
in-tree gmp, mpfr, mpc or isl, so pass `--with-gmp` and friends from the
devshell, as `/mnt/europa/project/flake.nix` does.

## 7. Gates

Two gate scripts live in `indy/`. Both start iris, drive the PROM, and wait for
a marker on the serial log. Both were run on 2026-11-17 with a 340-byte o32
test image and passed. The same image built as n32 with `-mtune=r4600` passed
the serial gate.

Serial gate (`indy/gate.sh IMAGE MARKER [TIMEOUT]`), for milestones without
graphics. It is `../redox/tools/indy-gate.sh` with the file name `haiku`:

```
banks = [64, 0, 0, 0]
no_audio = true
headless = true
nvram = "$WORK/nvram.bin"
[machine]
profile = "indy_ip24"
[network]
mode = "nat"
tftp_dir = "$WORK/tftp"
[scsi]
```

```
IRIS_NO_IDLE=1 IRIS_TFTP_TRACE=1 iris --config $WORK/machine.toml --ci \
  --ci-socket $WORK/i.sock --monitor-port $PORT --serial-log $WORK/serial.log \
  --headless --noaudio &
iris-ci --socket $WORK/i.sock -q status --wait-ready 30
iris-ci --socket $WORK/i.sock -q start
iris-ci ... serial-wait 'Option?' --timeout 60
iris-ci ... serial-send '5'
iris-ci ... serial-wait '>>' --timeout 30
iris-ci ... serial-send 'boot -f bootp()haiku'
iris-ci ... serial-wait 'entry: 0x' --timeout 60
iris-ci ... serial-wait "$MARKER" --timeout $TIMEOUT
```

Graphics gate (`indy/gate-gfx.sh IMAGE MARKER SCREENSHOT [TIMEOUT]`). The config
has no `headless` line and iris runs without `--headless`. The PROM is driven
through the monitor port:

```
mon() { printf '%s\n' "$*" | nc -w 4 127.0.0.1 $PORT; }
sleep 12; mon "ps2 esc"; sleep 2; mon "ps2 esc"
mon "screen wait 1500 40000"
mon "ps2 type 5"; mon "screen wait 1500 20000"
mon "ps2 type boot -f bootp()haiku"; mon "ps2 enter"
iris-ci ... serial-wait "$MARKER" --timeout $TIMEOUT
iris-ci ... screenshot $SCREENSHOT
```

GDB gate (`indy/gate-gdb.sh serial|udp`, or `make -C indy gate-gdb` for both).
It starts `remote_disk_server` with `generated.mips/disk.raw`, runs the serial
gate up to `Mounted boot partition`, then runs `indy/gdb-check.py` against the
stub. The serial mode uses `serial_socket_b` of iris, the UDP mode a
`[[port_forward]]` from host port 21590 to guest port 2159. The serial gate
takes such additions through `CONFIG_TOP`, `CONFIG_END` and `THEN`. iris drops
serial output that a socket client reads too slowly: a client must keep
reading while the kernel runs.

The default monitor ports are 18931 (serial gate) and 18932 (graphics gate).
A gate fails at once with "monitor cannot bind" in `iris.out` when another
iris holds its port: set `MONITOR_PORT`.

Rules for both: a gate passes only when the marker appears. Kill iris on exit.
Give concurrent runs different `$PORT` and `$WORK`. Keep `$WORK` short (section
2). To attach the Haiku disk image add `[scsi.1]` with `path`.

Kernel and loader debug output always goes to the serial port, so markers work
in both gates.

## 8. Milestones

### M0. Gate rig

**M0.1 Devshell.**
Read: `../os/flake.nix`, `/mnt/europa/project/flake.nix`, `ReadMe.Compiling.md`
lines 14 to 110.
Do: write `flake.nix` with one devshell `haiku-indy-dev`: host gcc 13, binutils,
gnumake, bison, flex, gawk, texinfo, autoconf, automake, perl, python3, git,
wget, curl, zlib and zstd with headers, gmp, mpfr, libmpc, isl, netcat, file.
Build Haiku's jam from a sparse fetch of the `jam/` directory of
github.com/haiku/buildtools (`make`, then `./jam0 -sBINDIR=$out/bin install`).
Done when: `nix develop -c jam -v` prints a version containing `-haiku-`.

**M0.2 ECOFF converter.**
Read: section 3.4, `../redox/tools/ecoff/src/main.rs`.
Do: write `build/scripts/elf2ecoff.py IN OUT [--append FILE]`. With `--append`:
write the bss as zero bytes, pad to a 4096-byte boundary, write a 4096-byte
header (`HAIKUARC`, then the payload length as a big-endian u32), then the
payload, pad to 4096, and set bsize to 0.
Done when: for the M0.3 ELF, the output without `--append` is byte-identical to
the output of `../redox/tools/ecoff/target/release/ecoff convert`.

**M0.3 Hello image.**
Read: sections 3.3 and 3.5.
Do: create `indy/m0/` with `start.S`, `hello.c`, `link.ld` and a `Makefile`. The
compiler is `$MIPS_CC`, default
`/mnt/europa/toolchains/mips64-n32--uclibc--stable-2026.08-1/bin/mips64-buildroot-linux-uclibc-gcc`.
Flags that worked: `-mabi=n32 -march=mips3 -mtune=r4600 -EB -msoft-float
-mno-abicalls -fno-pic -G0 -ffreestanding -nostdlib -O2`, linked with
`ld -m elf32btsmipn32 -T link.ld`. `link.ld` uses
`OUTPUT_FORMAT("elf32-ntradbigmips")`, base 0x88040000, `.text` with
`*(.text.start)` first, and a 16 KB stack inside `.bss`. `start.S` loads
`$sp` with `lui`/`addiu` and calls `hello_main` with `jal`. `hello_main` prints
`haiku: m0 alive\r\n` with the polled output of section 3.5 and loops forever.
Done when: `readelf -h` shows ELF32, big endian, entry 0x88040000, flags
containing `abi2, mips3`.

**M0.4 Gate scripts.**
Read: section 7, `../redox/tools/indy-gate.sh`.
Do: write `indy/gate.sh` and `indy/gate-gfx.sh`. Add `indy/Makefile` targets
`m0`, `gate-m0`, `gate-gfx-m0`, and `netboot` (copies an image to
`/var/lib/sgi-netboot/haiku`).
Done when: `make -C indy gate-m0` and `make -C indy gate-gfx-m0` both print
`PASS`, and the screenshot shows the PROM command monitor window.

### M1. Toolchain and build wiring

**M1.1 Public headers and config.**
Read: `headers/config/HaikuConfig.h`, the riscv64 and arm files in
`headers/posix/arch/` and `headers/os/arch/`.
Do: the `HaikuConfig.h` block from 5.3. Create `headers/posix/arch/mips/`
(`limits.h` with `PAGESIZE 16384`; `signal.h` with `struct vregs` holding 32
general registers, pc, hi and lo as `unsigned long long`, 32 doubles and fcsr;
`arch_setjmp.h` with room for 12 64-bit integer registers, 6 doubles and the
signal mask; `fenv.h`) and `headers/os/arch/mips/arch_debugger.h`. Make the
header changes marked [M1.1] in section 5.2, and the `core_dump.cpp` change.
Delete the files of section 5.1.
Done when: `grep -rn "mipsel\|MIPSEL" headers src/system src/kits src/tools build | grep -v gmp-impl.h`
prints nothing.

**M1.2 binutils.**
Read: section 6; in the fork `bfd/config.bfd` around line 778,
`ld/configure.tgt` around line 508, `ld/emulparams/elf_mipsel_haiku.sh`.
Do: the binutils additions of section 6. Build and install.
Done when: `mips-unknown-haiku-ld -V` lists `elf_mips_haiku` first, and
`mips-unknown-haiku-as` assembles a file with one `nop`.

**M1.3 GCC Haiku layer.**
Read: section 6. Fetch
`https://raw.githubusercontent.com/haikuports/haikuports/master/sys-devel/gcc/patches/gcc-15.3.0_2026_10_11.patchset`.
Do: apply the patch set to the `haiku-mips` worktree. Resolve each rejected hunk
by reading the surrounding GCC 17 code. Skip the libsanitizer hunks. Add the
MIPS parts of section 6.
Done when: `make all-gcc` succeeds for `--target=mips-unknown-haiku`, and
`xgcc -dumpmachine` prints `mips-unknown-haiku`.

**M1.4 Cross build script.**
Read: `build/scripts/build_cross_tools_gcc4`.
Do: write `indy/toolchain/build.sh`. It stages the Haiku headers, configures
and builds binutils and GCC (`all-gcc`, `all-target-libgcc`,
`all-target-libstdc++-v3`), and installs.
Done when: this prints two different paths that both exist:
`mips-unknown-haiku-gcc -print-libgcc-file-name` and
`mips-unknown-haiku-gcc -msoft-float -print-libgcc-file-name`. Also
`echo 'int x;' | mips-unknown-haiku-gcc -xc - -dM -E | grep -c "__HAIKU__\|_MIPSEB\|_ABIN32"`
prints 3 or more, and the same output has `__SIZEOF_POINTER__ 4` and
`__SIZEOF_LONG_DOUBLE__ 8` (or 16 if D1's fallback was taken).

**M1.5 Build system wiring.**
Read: section 5.2, and the riscv64 hits of the grep commands in 5.5.
Do: the `configure`, `ArchitectureRules`, `PackageArchitecture.h`,
`PackageInfo.cpp`, repository file and `kernel.h` changes of 5.2.
Done when: in `generated.mips`,
`../configure --cross-tools-prefix /mnt/europa/haiku-toolchain/cross-tools-mips/bin/mips-unknown-haiku-`
ends without error, and `jam -q "<build>package"` builds the host package tool.

**M1.6 Seed directory.**
Read: `build/jam/BuildFeatures` lines 42 to 95 and 388 to 407,
`build/jam/BuildFeatureRules` (`SetBuildFeatureAttribute`,
`BuildFeatureAttribute`, `EnableBuildFeatures`), `build/jam/SystemLibraryRules`.
Do: `indy/toolchain/seed.sh` fills `/mnt/europa/haiku-toolchain/seed` with the
soft-float `libgcc.a` and `libsupc++.a` (as `libgcc-kernel.a`,
`libsupc++-kernel.a`, `libgcc-boot.a`, `libsupc++-boot.a`), the default-multilib
static libraries, the C++ and GCC header directories, and the zlib 1.3.x sources
and headers. In `BuildFeatures`, when the architecture is mips and the package
is not available, set the same attribute names from the seed directory and
enable the feature.
Done when: jam no longer lists `gcc_syslibs_devel` or `zlib` in its
"build-feature packages unavailable" line.

**M1.7 Compile check.**
Do: compile one C file and one C++ file with the flags jam uses for kernel
code (`jam -dx` shows them once M3 has a Jamfile; until then use D5).
Done when: `readelf -h` on the objects shows `abi2, mips3`, and
`readelf -A` shows soft float for the kernel flags.

### M2. Loader (platform `arcs`)

Templates: `src/system/boot/platform/riscv/` for structure,
`src/system/boot/platform/u-boot/devices.cpp` lines 27 to 47 for the
`MemoryDisk` boot device.

**M2.1 Skeleton that links and prints.**
Read: `headers/private/kernel/boot/platform.h`, `src/system/boot/Jamfile`
lines 340 to 470, `src/system/boot/platform/riscv/Jamfile`, `start.cpp`,
`crt0.S`, `src/system/ldscripts/riscv64/boot_loader_riscv.ld`.
Do: `src/system/boot/platform/arcs/` with `crt0.S`, `start.cpp`, `debug.cpp`
(serial `dprintf` and `panic`), `console.cpp` (text console on the serial port),
stubs for the other functions in `platform.h`. `src/system/boot/arch/mips/` with
a Jamfile like riscv64's. `src/system/ldscripts/mips/boot_loader_arcs.ld` at
0x88040000 with `OUTPUT_FORMAT("elf32-ntradbigmips")`, `.got` and `_gp`. A
`platform_kernel_args.h` in `headers/private/kernel/boot/platform/arcs/` with
`SMP_MAX_CPUS 1` and the three range limits set to 32. `BuildEcoffLoader` in `src/system/boot/Jamfile` runs `elf2ecoff.py`.
Keep `.bss` small: allocate the stack and heap from RAM at run time.
Done when: `jam -q haiku_loader.arcs` builds, and the serial gate sees
`haiku_loader: start`.

**M2.2 Memory.**
Read: section 3.2, `src/system/boot/platform/riscv/mmu.cpp`.
Do: read MEMCFG, record RAM in `gKernelArgs.physical_memory_range`. Record
physical 0x0800_0000 to the end of the loaded image as allocated. Implement
`platform_allocate_heap_region` and a physical allocator that hands out 16 KB
pages downward from the top of RAM. Reserve physical 0x0900_0000 to 0x097F_FFFF
for the kernel image and panic if the allocator reaches it. Panic below 64 MB.
Done when: the gate log shows the RAM size and the image end address, and the
loader reaches `main()`.

**M2.3 Archive as boot volume.**
Read: `src/system/boot/platform/u-boot/devices.cpp`,
`src/system/boot/loader/vfs.cpp` lines 290 to 310 and 700 to 750,
`src/system/kernel/fs/vfs_boot.cpp` lines 186 to 300 (how the kernel uses the
values of D10), `build/jam/images/NetBootArchive`.
Do: find the `HAIKUARC` header at the first 4096-byte boundary after the
loader's `_end`. `platform_add_boot_device` adds a `MemoryDisk` over the payload.
`platform_get_boot_partitions` returns its first partition.
`platform_register_boot_device` sets the values of D10. Build the netboot file:
`elf2ecoff.py boot_loader_arcs haiku --append haiku-netboot.tgz`. Fail the rule
if the result exceeds 12 MB.
Done when: with an archive holding one test file, the gate log shows
`haiku_loader: archive mounted` and the file name.

**M2.4 Page tables and kernel regions.**
Read: decisions D6, D7 and D9, `src/system/boot/loader/elf.cpp` lines 296 to 330.
Do: `platform_allocate_region` returns kseg0 addresses for the loader's use and
records a kseg2 mapping from 0xC000_0000 upward in loader-built page tables. A
request at the kernel's link address returns that kseg0 address with no kseg2
mapping. Implement `platform_bootloader_address_to_kernel_address`. Put the page
directory address in `arch_kernel_args`. Report the virtual range of D7.
Done when: a self-test at the start of `platform_start_kernel` installs the
refill vector, writes a word through kseg0, reads it back through its kseg2
address and prints `haiku_loader: kseg2 ok`.

**M2.5 Kernel entry.**
Read: `src/system/boot/platform/riscv/start.cpp` lines 107 to 190, `entry.S`.
Do: `platform_start_kernel`: flush the D-cache and I-cache, install the TLB
refill vector at 0x8000_0000 and 0x8000_0080, clear `Status.BEV`, set `$sp` to
the top of `cpu_kstack[0]`, `$a0` to the kernel_args address, `$a1` to 0, `$t9`
to the entry, and jump.
Done when: with the M3.1 kernel in the archive, the gate log shows the kernel's
first line.

**M2.6 Loader arch ELF.**
Read: `src/system/kernel/arch/riscv64/arch_elf.cpp`,
`src/system/boot/arch/riscv64/Jamfile`.
Do: `src/system/kernel/arch/mips/arch_elf.cpp` with a `_BOOT_MODE` half:
`boot_arch_elf_relocate_rel` accepts `R_MIPS_NONE` and `R_MIPS_REL32` when the
load delta is 0 and fails otherwise. `boot_arch_elf_relocate_rela` fails. Define
the `R_MIPS_*` and `DT_MIPS_*` constants in
`headers/private/system/arch/mips/arch_elf.h` from the MIPS psABI.
Done when: the loader loads the kernel ELF without a relocation error.

### M3. Kernel core

Templates: `src/system/kernel/arch/riscv64/` for file layout and for everything
that handles 64-bit registers and eight argument registers.
`../redox/recipes/core/kernel/indy-mips64.patch` for MIPS n32 assembly that
runs on iris (exception entry, TLB writes, context switch). The functions to
implement are the declarations in `headers/private/kernel/arch/*.h`. When code
copied from riscv64 stores an `addr_t` or `uint32` into a register slot, apply
the sign-extension rule of D1.

**M3.1 Kernel that links and prints.**
Read: `src/system/kernel/arch/riscv64/Jamfile`,
`src/system/ldscripts/m68k/kernel.ld`, `src/system/kernel/platform/u-boot/`.
Do: `src/system/ldscripts/mips/kernel.ld` at `0x89000000 + SIZEOF_HEADERS` with
`OUTPUT_FORMAT("elf32-ntradbigmips")`, `.got` and `_gp`. `src/system/kernel/platform/arcs/platform.cpp`. Every file of
the riscv64 arch directory as a mips file whose functions panic with their own
name, except `arch_debug_console.cpp` (section 3.5) and `arch_platform.cpp`.
Headers in `headers/private/kernel/arch/mips/` and
`headers/private/system/arch/mips/`: `arch_kernel.h` from D6, `arch_vm.h` with
`PAGE_SHIFT 14`, `arch_config.h` like riscv64's (no atomic syscall defines),
`arch_atomic.h` with `ll`/`sc` and `lld`/`scd` loops, `arch_kernel_args.h` with
the page directory address. `src/tools/gensyscalls/arch/mips/
arch_gensyscalls.h` with `long long` for the return and parameter alignment
types, because every n32 argument slot is 8 bytes.
Done when: `jam -q kernel_mips` links, `readelf -h` shows type `EXEC` and entry
in 0x89xx_xxxx, and the gate log shows `Welcome to kernel debugger output!`.

**M3.2 Vectors and trap frame.**
Read: decisions D1 and D8, `src/system/kernel/arch/riscv64/arch_traps.S` and
`arch_int.cpp`, the exception entry in the Redox patch.
Do: `arch_asm.S` with the vector templates and the general handler of D8, an
`iframe` struct with 32 registers, hi, lo, epc and badvaddr as `uint64` and
status and cause as `uint32`, `asm_offsets.cpp`. `arch_int_init` copies the
templates to 0x8000_0000, 0x8000_0080 and 0x8000_0180, installs a panic stub at
the cache error vector 0xA000_0100, and invalidates the I-cache there. An exception
with no handler prints all registers and enters the kernel debugger.
Done when: a deliberate read of address 0x0000_0010 from kernel code prints a
register dump with `badvaddr 0x00000010`.

**M3.3 TLB and translation map.**
Read: decisions D6 and D9,
`src/system/kernel/arch/riscv64/RISCV64VMTranslationMap.cpp`,
`arch_vm_translation_map.cpp`, `arch_vm.cpp`,
`headers/private/kernel/vm/VMTranslationMap.h`.
Do: `MIPSVMTranslationMap`, the physical page mapper over kseg0, the functions
of `arch/vm_translation_map.h` and `arch/vm.h`, the TLB primitives (write
indexed, write random, probe and invalidate one address, flush all), the slow
path for TLB invalid and modify exceptions that calls `vm_page_fault`. Map
device memory uncached (EntryLo cache field 2) and RAM cached (field 3).
Done when: the kernel passes `vm_init` and prints the page counts.

**M3.4 Threads and context switch.**
Read: `src/system/kernel/arch/riscv64/arch_thread.cpp`, `arch_asm.S`.
Do: `arch_thread.cpp` kernel-thread parts, `arch_context_switch` saving `$16`
to `$23`, `$28`, `$29`, `$30`, `$31` (64-bit) and `Status`; update the kernel
stack pointer in the exception block on every switch.
Done when: two kernel threads print alternately.

**M3.5 Timer and time.**
Read: decision D12, `src/system/kernel/arch/riscv64/arch_timer.cpp`,
`src/system/kernel/lib/arch/riscv64/Jamfile`.
Do: `arch_timer.cpp`, the Count calibration against 8254 counter 2 (section
3.6 addresses: program a count, read Count before and after it expires),
`system_time()` for the kernel, IP7 dispatch.
Done when: the boot log shows a Count rate within 1 percent of 33 MHz, and a
kernel thread that calls `snooze(1000000)` in a loop prints once per second for
10 seconds, checked against the gate's wall clock.

**M3.6 Interrupt controller.**
Read: section 3.6, decision D13.
Do: IP2 and IP3 dispatch through INT3 with the vector numbers of D13,
`arch_int_enable_io_interrupt` and `arch_int_disable_io_interrupt` on the mask
registers, a spurious-interrupt counter per level.
Done when: a test handler on vector 53 (serial, mapped bit 5) counts
interrupts, and after `iris-ci serial-send x` the kernel prints a count of 1 or
more.

**M3.7 Remaining arch functions.**
Do: `arch_cpu.cpp` (cache routines from sections 3.13 and 3.16 with line size
and ways read from Config and PRId, always addressed through kseg0; TLB
invalidation; shutdown), `arch_real_time_clock.cpp` (section 3.10),
`arch_system_info.cpp` (`B_CPU_MIPS`), `arch_debug.cpp` stack traces (saved
`$ra` and `$30` are 64-bit slots), `arch_smp.cpp`, `arch_user_debugger.cpp`
returning errors.
Done when: `grep -rn "panic(\"arch_" src/system/kernel/arch/mips` lists only
user-mode functions, and the boot log shows the RTC date.

**M3.8 Gate.**
Done when: the serial gate shows `haiku: m3 threads ok`, printed by a temporary
test in `arch_platform_init_post_thread` after M3.4 and M3.5 pass, and the
kernel then stops in the kernel debugger for lack of a boot volume.

### M4. Modules, interrupts, storage

**M4.1 MIPS relocation for add-ons.**
Read: `src/system/kernel/elf.cpp` lines 823 to 925 and 1202 to 1246, the MIPS
psABI chapter on the global offset table.
Do: in `arch_elf.cpp`, a function that reads `DT_PLTGOT`, `DT_MIPS_LOCAL_GOTNO`,
`DT_MIPS_GOTSYM` and `DT_MIPS_SYMTABNO` from the image's dynamic section, adds
the load delta to local GOT entries after the first two, and resolves each
global GOT entry from the dynamic symbol with the same index offset. Call it
from `elf_relocate` under `#ifdef __mips__`. `arch_elf_relocate_rel` handles
`R_MIPS_REL32`: add the delta for symbol indices below `DT_MIPS_GOTSYM`, add the
symbol value otherwise. After relocating an image call `arch_cpu_sync_icache`
on its text.
Done when: the boot log shows a preloaded add-on's `init` running.

**M4.2 Root bus.**
Read: `src/add-ons/kernel/bus_managers/fdt/` (how it attaches to the root node
and registers children), `headers/os/drivers/device_manager.h`.
Do: `bus_managers/indy`: attaches to the device manager root and registers one
child node per fixed device with a name attribute and a fixed child driver.
Done when: the kernel debugger command `dm_tree` lists the indy nodes.

**M4.3 SCSI controller, polled.**
Read: section 3.12, `../os/kernel/src/scsi.rs` (facts only), the smallest
driver under `src/add-ons/kernel/busses/scsi/`.
Do: `busses/scsi/wd33c93_hpc3`: a SIM for the SCSI bus manager that executes
one command at a time with polled PIO, target IDs 1 to 7.
Done when: with a raw image attached as `[scsi.1]`, the boot log shows the disk
size.

**M4.4 Boot volume.**
Do: add `scsi`, `scsi_disk`, `wd33c93_hpc3`, `indy`, `bfs`, `packagefs` and the
`intel` partitioning system to the netboot archive for mips. Create an empty BFS
image with the build's `bfs_shell`.
Done when: the gate with that image shows the BFS volume mounted, and prints
`haiku: m4 boot volume mounted` from a temporary line in the boot path.

**M4.5 SCSI interrupts and DMA.**
Do: switch command completion to the SCSI 0 interrupt (local 0 bit 1). Add HPC3
DMA with bounce buffers, the rules of section 3.12, and cache maintenance:
writeback before device reads memory, invalidate after device writes memory,
both with the loop rule of section 3.16.
Done when: reading the whole 64 MB image gives the same checksum as the host
file, in PIO and DMA modes.

### M5. Userland

**M5.1 User mode in the kernel.**
Read: `src/system/kernel/arch/riscv64/arch_thread.cpp`, `arch_int.cpp`,
`arch_commpage.cpp`, decisions D1, D2 and D11.
Do: syscall entry. The number is in `$2`. Arguments are in `$4` to `$11` as
eight 64-bit slots; when the syscall's parameter size exceeds 64 bytes, copy
the rest from the user stack starting at 0(`$sp`). Store the result in `$2`:
as is for a 64-bit return type, sign-extended from 32 bits for every smaller
type (the generated `kExtendedSyscallInfos` table gives each return size). Add
4 to EPC. User threads get `Status` with user mode, `UX` and interrupts
enabled. Then `arch_thread_enter_userspace`, signal frames,
`arch_thread_init_tls`, the `$k1` rule, lazy FPU, `rdhwr` emulation, user
`memcpy` fault handling.
Done when: the kernel starts a static test program that makes one syscall.

**M5.2 libroot and glue.**
Read: the userland table in section 5.3, and the riscv64 files of each
directory in it.
Do: `glue/arch/mips` (crti.S, crtn.S), `libroot/os/arch/mips` (`syscalls.inc`
with `li $2, n; syscall; jr $31`, byteorder, `tls.c` reading `$27`,
`system_time` through `_kern_system_time`, atomics with `ll`/`sc` and
`lld`/`scd`), `libroot/posix/arch/mips` (setjmp and longjmp saving `$16` to
`$23`, `$28`, `$29`, `$30`, `$31` as 64-bit values and `$f20`, `$f22`, `$f24`,
`$f26`, `$f28`, `$f30`; fenv), string, glibc and musl arch directories, the
`syscalls.S` branch, the malloc page shift.
Done when: `jam -q libroot.so` links.

**M5.3 runtime_loader.**
Read: `src/system/runtime_loader/arch/riscv64/arch_relocate.cpp`, task M4.1.
Do: `arch_relocate_image` for MIPS: the GOT step of M4.1, `R_MIPS_REL32`,
`R_MIPS_TLS_DTPMOD32`, `R_MIPS_TLS_DTPREL32`. Resolve all symbols at load time.
Done when: `jam -q runtime_loader` links.

**M5.4 Shared GCC libraries.**
Read: `docs/develop/packages/Bootstrapping.rst`,
`build/jam/packages/HaikuCrossDevel`.
Do: rebuild the GCC target libraries with shared libraries enabled against a
sysroot that holds the Haiku headers and the stub libroot
(`src/system/libroot/stubbed`). Package the results as `gcc_syslibs` and
`gcc_syslibs_devel` with the host `package` tool.
Done when: `package list` on both files shows `libgcc_s.so.1`, `libstdc++.so`
and the static libraries.

**M5.5 Third-party packages.**
Read: `docs/develop/packages/Bootstrapping.rst`,
`build/jam/repositories/HaikuPortsCross/riscv64`.
Do: clone haikuporter, haikuports.cross and haikuports under
`/mnt/europa/haiku-toolchain`. Add `mips` to haikuporter's architecture table
and to the `ARCHITECTURES` line of each recipe in the riscv64 stage 1 list. Use
a second output directory configured with `--bootstrap`. Place the M5.4 packages
where the bootstrap repository expects its stage 0 packages, so haikuporter does
not build GCC. Build one package at a time, zlib first.
Done when: the packages named in `HaikuPortsCross/mips` exist as `.hpkg` files.

**M5.6 Image.**
Do: build `haiku.hpkg` and a raw image with the minimum profile. Add a launch
job or boot script line that writes `haiku: m5 userland` to `/dev/dprintf`.
Done when: the serial gate with that image shows the marker.

### M6. Desktop

**M6.1 REX3 pixel upload.**
Read: section 3.8, `../iris/src/dev/ng1/rex3_generic.rs` (`host_count`,
`host_pack`), `../os/kernel/src/gfx.rs`.
Do: extend `indy/m0` with a test that loads a palette and uploads a 256 x 256
gradient through HOSTRW0, once with 32-bit stores (4 pixels each) and once
beside it with 64-bit stores and `rwdouble` (8 pixels each).
Done when: the graphics gate screenshot shows both gradients at the expected
positions.

**M6.2 Kernel driver.**
Read: `src/add-ons/kernel/drivers/graphics/framebuffer/` for the driver shape.
Do: `drivers/graphics/newport` publishes `/dev/graphics/newport`, maps the REX3
registers, creates the shadow buffer area, and offers ioctls to clone both
areas and to set palette entries.
Done when: the device appears in `/dev/graphics` in the boot log.

**M6.3 Accelerant.**
Read: `src/add-ons/accelerants/framebuffer/` and
`src/add-ons/accelerants/vesa/hooks.cpp`.
Do: `accelerants/newport` with the required hooks, `B_SET_INDEXED_COLORS`, and
the flush hook, which uploads one rectangle from the shadow as in M6.1.

**M6.4 app_server flush hook.**
Read: `src/servers/app/drawing/HWInterface.cpp` lines 339 to 400 and 650 to 880,
`interface/local/AccelerantHWInterface.cpp` lines 340 to 430 and 1360 to 1380.
Do: a virtual method called at the end of `HWInterface::_CopyToFront` with the
rectangle. `AccelerantHWInterface` looks up one private optional hook and calls
it from that method.
Done when: the graphics gate screenshot shows the Haiku desktop background.

**M6.5 Keyboard and mouse.**
Read: decision D15, `src/add-ons/kernel/bus_managers/isa/arch/m68k/`,
`src/add-ons/kernel/bus_managers/ps2/ps2_common.cpp` lines 140 to 160 and 400
to 430.
Do: `isa/arch/mips`, the ps2 mips branch, the image list entries.
Done when: `ps2 type` through the iris monitor produces text in a Terminal
window in the screenshot, and `ptr move` moves the cursor.

**M6.6 Gate.**
Done when: the graphics gate shows `haiku: m6 desktop` (written by a boot script
after Deskbar starts) and the screenshot shows Deskbar and a Terminal window
with correct colours.

### M7. Network

**M7.1 Driver.** Read: section 3.11 and 3.6, a small native driver under
`src/add-ons/kernel/drivers/network/ether/`. Do: `seeq8003` publishing
`/dev/net/seeq8003/0`, MAC from NVRAM, 8 receive descriptors, the acknowledge
order of section 3.6, cache maintenance on every buffer.
**M7.2 Gate.** Done when: the image gets a DHCP lease from iris, pings
192.168.0.1, and a boot script writes `haiku: m7 ping ok`.

### M8. Physical Indy

Each item needs the owner at the machine.

- Check which TFTP file the PROM requests (section 2).
- Run all gates under iris with `cpu = "r5000"` first: it has the R4600's line
  size and two ways.
- Print PRId, Config and the measured Count rate at boot, and confirm the
  R4600 revision handling of section 3.16.
- Run a cache stress test: SCSI reads and writes under load, with checksums.
- Confirm the SCSI DMA direction bit, the 8042 translation behaviour and the
  64-bit REX3 host writes.
- Decide the root volume: a second SCSI disk, or RemoteDisk.

### M9. Performance on the R4600

Each task records a measurement before and after in section 1. Do them after
M6, in this order.

**M9.1 ASIDs.** Give each address space an 8-bit ASID with a generation
counter. Write EntryHi with the ASID on a switch. Flush the TLB only when the
ASID space wraps. Done when: a test with two teams that exchange messages shows
fewer TLB refills per switch, and a stress test of 300 short-lived teams passes.

**M9.2 Wired kernel stack.** Map the current thread's kernel stack page with
TLB entry 0 and set `Wired` to 1. Rewrite the entry on every context switch.
The exception handler then saves registers straight to the kernel stack and the
copy of D8 goes away. Keep a check that sends a fault on the kernel stack itself
to a static emergency stack. Done when: the time for an empty syscall drops.

**M9.3 User-mode clock.** Calibrate the RPSS counter (section 3.7) against the
8254. Map the memory controller page read-only into every team. Publish a
versioned pair (counter value, system time) and the conversion factor in the
commpage and refresh it at least every 30 seconds. `system_time()` in libroot
reads the counter and the pair without a syscall. Done when: `system_time()`
agrees with `_kern_system_time` within 100 microseconds over an hour-long test,
and a loop of one million calls runs at least five times faster.

**M9.4 Hardware cursor.** Implement the cursor hooks of the accelerant with the
VC2 cursor (`../os/kernel/src/gfx.rs` has the VC2 register numbers). Done when:
moving the pointer causes no REX3 uploads.

**M9.5 SCSI throughput.** Measure sequential read speed with DMA. Raise the
transfer size per command until the speed stops improving.

**M9.6 R4600SC secondary cache.** Read Linux `arch/mips/mm/sc-ip22.c` for the
probe, enable and invalidate sequences. Enable the cache and add its
invalidation to the DMA cache routines. Only with the owner at the machine.

### M10. Later

RemoteDisk root, disk boot through an SGI volume header (firmware calls need
the thunk of D1), VC2 mode setting without the PROM, HAL2 audio, FPU emulation
of unimplemented operations.

## 9. Risks and open problems

- **GCC 17 with this tree.** Upstream builds with GCC 13. Expect new errors
  from GCC 14 and later (implicit declarations, pointer type mismatches). Fix
  Haiku's own code. For vendored glibc code add `-fpermissive` to that directory
  only.
- **16 KB pages are new to Haiku.** Review every hit of
  `grep -rn "\b4096\b" src/system` when a size looks wrong.
- **PageMask 0x6000 under iris in 32-bit mode** has not been exercised by any
  project here. M2.4 tests it first.
- **Big-endian userland.** No big-endian Haiku port has reached the desktop.
  Expect byte-order bugs in app_server colour handling and in file formats.
  BFS is little-endian on disk on every host and HPKG is big-endian, so images
  built on x86 are readable. ICU data must be the big-endian variant.
- **Keyboard scancode set** on the real controller (section 3.9).
- **Haiku's ps2, SCSI and network stacks on a bus that is neither PCI nor FDT.**
  Task M4.2 is the first test of the fixed-child approach.
- **32-bit pointers with 64-bit registers are new to Haiku.** Watch every place
  where code stores a register value in an `addr_t` or loads one from it, and
  apply the sign-extension rule of D1.
- **R4600 cache errata** (section 3.16) cannot be reproduced on iris. The loop
  rule is applied on every CPU so the code path under test is the one the
  machine runs.
- **R4600 FPU** raises Unimplemented Operation for denormal operands.
- **Display speed.** A full-screen flush is 327,680 register writes.
- **`Status.BEV` at PROM entry** has not been read. The loader must not depend
  on its value.
- **The macro `mips`.** GCC predefines `mips` as 1 in the GNU dialects, also
  `MIPSEB`, `R4000` and `LANGUAGE_C`. `HaikuConfig.h` undefines `mips`, because
  the name is a path component of `<arch/mips/...>` includes. Do not name an
  identifier `mips` or `R4000`. In `.S` files a CP0 register is written
  `C0_STATUS`; `$CP0_STATUS` is one token and is not expanded.
- **Instruction cache and file pages.** The kernel fills pages through kseg0,
  so new code is in the data cache. `MIPSVMTranslationMap::Map` writes back and
  invalidates the page's cache lines for every executable mapping. That is
  correct and slow. A page that is written again while it is mapped executable
  is not handled.
- **Address spaces share ASID 0.** Every address space switch flushes the TLB
  (M9.1 removes that). User pages are readable from user mode whatever their
  protection says: the TLB has no user bit.
- **Bus errors.** `arch_cpu_probe_read32()` relies on the Data Bus Error
  exception. iris raises it for a missing REX3. If the machine reports the error
  as interrupt IP6 instead, the kernel panics in the newport driver's probe.
- **Stack traces** list every kernel return address found on the stack. MIPS
  code has no frame pointer chain.
- **SCSI is polled PIO.** A request blocks the caller until it is done.
- **Seed directory and the bootstrap build** (M1.6, M5.4, M5.5) follow the
  mechanisms in the build rules but have not been run for any architecture
  here. Expect to adjust the steps.

Open problems found during work are appended below with the date.

## 10. References

- This tree: `docs/develop/packages/Bootstrapping.rst`,
  `docs/develop/kernel/arch/sparc/overview.rst` (notes from another unfinished
  big-endian port), `docs/develop/kernel/system_calls.rst`.
- `../iris/HELP.md`, `debug.md`, `docs/ci.md`, `docs/interrupt_map.md`,
  `src/dev/ioc.rs`, `src/dev/ps2.rs`, `src/dev/ng1/`.
- `../redox/bootstrap.md`, `docs/adr/0003-paging-and-tlb.md`,
  `recipes/core/kernel/indy-mips64.patch`.
- `../os/kernel/src/` (`gfx.rs`, `kbd.rs`, `rtc.rs`, `scsi.rs`, `cache.rs`),
  `../os/crates/irix7-seeq/`, `../os/docs/research/rx-interrupt.md`.
- `/mnt/europa/project/docs/toolchain.md`, `docs/gcc-cross-build.md`,
  `scripts/build-toolchain.sh`.
- MIPS R4000 user's manual: `../iris/docs/R4000_um2.pdf`. MIPS psABI
  (System V ABI, MIPS RISC Processor Supplement) for ELF relocation rules.
