# piece1-kext: ElyanMI50 -- the IOKit skeleton (runs on hardware)

The first code that runs on the real G5: a Darwin/XNU kernel extension that
matches an AMD MI50 (Vega 20 / gfx906) over PCI, maps its BARs, and dumps the
ATOM VBIOS signature. It is the bridge between the proven piece-1 ATOM
interpreter and the actual card.

## What start() does
1. OSDynamicCast the provider to IOPCIDevice; log vendor:device.
2. setMemoryEnable + setBusMasterEnable.
3. Map BAR0 (MMIO). The ATOM card_info reg callbacks will wrap this with
   OSReadLittleInt32/OSWriteLittleInt32 -- the big-endian boundary.
4. Enable + map the expansion ROM (VBIOS), read the 0xAA55 signature and the
   ATOM_ROM_TABLE_PTR (0x48) using byte-assembled reads (BE-safe).

First milestone: `kextload` it, `ioreg` shows ElyanMI50 attached to the GPU, and
the system log prints the ROM signature `aa55`. That is "Darwin attached to a
modern GPU on PowerPC."

## Build (on the G5 -- NOT off-target on Linux)
Needs the Mac OS X 10.5 Kernel SDK (Kernel.framework / IOKit headers). This will
not build on a Linux box; it targets the PowerPC XNU kernel.

    # with Xcode 3.x on Leopard, or by hand against the 10.5 Kernel SDK:
    #  - compile ElyanMI50.cpp with the kernel C++ flags (-mkernel, no exceptions/RTTI)
    #  - link as a .kext bundle with Info.plist
    #  - sudo kextload ElyanMI50.kext ; check `ioreg -c IOPCIDevice` and the log

## Confirm your PCI ID
Info.plist matches 0x66a11002 (MI50/MI60). Vega 20 ships as 0x66a0/0x66a1/0x66a3/
0x66a7/0x66af -- check your card and adjust IOPCIMatch if needed.

Built by Elyan Labs.
