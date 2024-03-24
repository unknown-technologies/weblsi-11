LSI-11 in the Browser
=====================

This is an emulator of an LSI-11 system (PDP-11/03-L) connected to a VT240
emulator, all running in the browser.

The emulated system has 6 modules installed:
- KD11-NA (M7270) CPU
- MSV11 (M8044) 32KW RAM
- RXV21 (M8029) RX02 floppy disk controller
- RLV12 (M8061) RL01/RL02 hard disk controller
- DLV11-J (M8043) 4x Serial line interface
- BDV11 bus terminator, bootstrap, and diagnostic ROM

This emulator passes (at least) the basic instruction test (VKAAC0), EIS
instruction test (VKABB0), FIS instruction test (VKACC1), traps test (VKADC0),
and 4K system exerciser (VKAHA1). It can boot RT-11SB V05.07, RT-11FB V05.07,
and XXDP V2.6. A pre-configured version of XXDP with all relevant diagnostics
is included in `rx02/lsixxdp.rx2`.

The VT240 terminal emulator is taken from the separate
[VT240 repository](https://github.com/unknown-technologies/vt240) which also
includes an optional native version of the VT240 terminal emulator.


Build Requirements
------------------

For the web version:
- Linux with git and make
- emscripten
- glslang


Building
--------

Just run `make` to compile an LSI-11 emulator without any disks. If you want to
include a particular floppy disk from the `rx02` folder or a hard disk from the
`rl02` folder, place the relevant disk images in the corresponding folder and
pass the arguments `RX2IMG0`, `RX2IMG1`, `RL2IMG`, and `BOOTDEV` to `make`.

For example, to include a floppy disk image `rx02/lsixxdp.bin` and
automatically boot from the RX02 floppy disk drive, you could use the following
make command line:

```sh
make RX2IMG0=lsixxdp BOOTDEV=RX02
```

To build an image with `rx02/lsixxdp.bin` and `rl02/rt11v57.bin` with the boot
device set to the RL02 disk, run the following command:

```sh
make RX2IMG0=lsixxdp RL2IMG=rt11v57 BOOTDEV=RL02
```

Once the build process finishes, the compiled code is available in
`build/weblsi-11.js` and `build/weblsi-11.wasm`. These two files have to be
deployed on a web server, together with an index file which loads them.


Keyboard
--------

A VT240 does not have all the keys you usually find on a PC keyboard and some
keys have a very different meaning. In particular, the function keys F1-F5 are
"local" keys which have no associated sequence that could be sent to the host.

The VT240 emulator uses the following map for local keys:
- F2 = toggle fullscreen
- F3 = setup
- F5 = send BREAK which HALTs the CPU


How to boot a PDP-11/03
-----------------------

When the system boots, the boot ROM asks `START?`. You can answer with `Y` to
boot from the default boot device, or enter a device name. Since this machine
only has an RX02 floppy disk drive and an RL02 hard disk drive, you can enter:

- `DY0`: first RX02 floppy disk
- `DY1`: second RX02 floppy disk
- `DL0`: first RL02 hard disk
- `DL1`: second RL02 hard disk
- `DL2`: third RL02 hard disk
- `DL3`: fourth RL02 hard disk

Keep in mind that the LSI-11 emulator only emulates a single hard disk, so you
can only ever boot from `DL0`, and only if the emulator was built with an RL02
disk image. Similarly, you can only boot from an RX02 floppy disk if a bootable
floppy was included when building the emulator.


PDP-11/03 ODT
-------------

The LSI-11 has built-in microcode in the CPU which implements a minimal
debugger. This debugger is invoked whenever the CPU HALTs, which means whenever
you send a BREAK via the terminal or when the CPU encounters a HALT
instruction. Once ODT is entered, the CPU prints the current value of the `PC`
register followed by a newline and `@`. You can enter various commands at this
prompt:

- Query registers values via `R0/`. The CPU prints the current contents of the
  register. If you enter a number here, this value is stored in the register.
  If you only press ENTER, nothing is changed. Valid registers are `R0` - `R7`
  as well as `RS` for the status word.
- Query memory contents via `177522/`. The CPU prints the current contents of
  the memory location `177522` (and of course you can enter any memory
  location). If you enter a new number, this value is stored in memory,
  otherwise it is left unchanged. If the memory location does not exist, the
  CPU prints a `?` and ignores the command.
- Proceed (continue) with program execution: `P`. The moment you enter the `P`,
  the CPU continues executing the program code starting at `PC`.
- Run arbitrary code with `152010G` where `152010` is the start address of the
  code. Again, the moment you type `G`, the CPU immediately starts executing
  the code.
- A few more things that may or may not be implemented in this emulator. You
  can read more about it in the ODT section of the LSI-11 processor handbook.


Basics of XXDP
--------------

XXDP is the official diagnostic system for the PDP-11 series of computers, with
its own file system and commands and everything. Essentially it can run
programs in absolute loader format and it provides some minimal monitor
specifically designed to support diagnostic programs. Once you booted XXDP, you
can run various commands. Since the LSI-11 has only 56kB RAM and no MMU, XXDP
starts with the small monitor which has a very limited feature set. Remember
the line with `RESTART ADDRESS: 152010` because if you want to cancel one of
the basic diagnostics, you have to HALT the CPU (press F5 in the browser), then
once you see the `@` prompt, enter the address directly followed by `G` to
restart XXDP without rebooting the system.

The most important commands are:

- `D`: show files on the disk
- `R FILE`: run file, like e.g. `R VKADC0` to run the `VKADC0` test

If you like to suffer, there is a minimal editor called `XTECO` included on the
disk. You can run it via `R XTECO`, but you will definitely have to carefully
read the XXDP manual to get any idea how to interact with this piece of
software from hell.


Online Demo
-----------

A compiled version of this emulator with the lsixxdp floppy disk in RX02 unit 0
(DY0) and RT-11FB V05.07 in RL02 disk 0 (DL0) is available here:
[https://lsi-11.unknown-tech.eu](https://lsi-11.unknown-tech.eu)
