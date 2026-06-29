#include "ElyanMI50.h"
#include <IOKit/IOLib.h>

#define super IOService
OSDefineMetaClassAndStructors(ElyanMI50, IOService)

bool ElyanMI50::start(IOService *provider)
{
    if (!super::start(provider))
        return false;

    fPci = OSDynamicCast(IOPCIDevice, provider);
    if (!fPci) {
        IOLog("ElyanMI50: provider is not an IOPCIDevice\n");
        return false;
    }

    UInt16 vid = fPci->configRead16(kIOPCIConfigVendorID);
    UInt16 did = fPci->configRead16(kIOPCIConfigDeviceID);
    IOLog("ElyanMI50: matched %04x:%04x (Vega 20 expected 1002:66xx)\n", vid, did);

    fPci->setMemoryEnable(true);
    fPci->setBusMasterEnable(true);

    /* BAR0 = MMIO register aperture. The ATOM card_info reg_read/reg_write
     * callbacks will wrap this with OSReadLittleInt32/OSWriteLittleInt32
     * (the big-endian boundary -- see piece1-atom-post). */
    fBar0 = fPci->mapDeviceMemoryWithRegister(kIOPCIConfigBaseAddress0);
    if (fBar0)
        IOLog("ElyanMI50: BAR0 MMIO @ 0x%lx len 0x%lx\n",
              (unsigned long)fBar0->getVirtualAddress(),
              (unsigned long)fBar0->getLength());
    else
        IOLog("ElyanMI50: BAR0 map failed\n");

    /* Enable and map the PCI expansion ROM (the ATOM VBIOS), then read the
     * 0xAA55 signature using BYTE-ASSEMBLED reads -- endian-agnostic, the same
     * trick that makes the ATOM interpreter BE-safe. */
    UInt32 rombar = fPci->configRead32(kIOPCIConfigExpansionROMBase);
    fPci->configWrite32(kIOPCIConfigExpansionROMBase, rombar | 0x1 /* ROM enable */);
    fRom = fPci->mapDeviceMemoryWithRegister(kIOPCIConfigExpansionROMBase);
    if (fRom) {
        volatile UInt8 *rom = (volatile UInt8 *)fRom->getVirtualAddress();
        UInt16 sig = (UInt16)(rom[0] | (rom[1] << 8));            /* expect 0xAA55 */
        UInt16 atom_tbl = (UInt16)(rom[0x48] | (rom[0x49] << 8)); /* ATOM_ROM_TABLE_PTR */
        IOLog("ElyanMI50: ROM sig 0x%04x (expect aa55), ATOM table ptr 0x%04x\n",
              sig, atom_tbl);
        /* Next: copy the ROM into kernel memory and hand it to
         * amdgpu_atom_parse() (piece 1 interpreter) to run ASIC_INIT. */
    } else {
        IOLog("ElyanMI50: expansion ROM map failed\n");
    }
    /* restore ROM-disable so we don't leave the decode on */
    fPci->configWrite32(kIOPCIConfigExpansionROMBase, rombar);

    registerService();
    IOLog("ElyanMI50: started (Darwin talking to a modern GPU on PowerPC)\n");
    return true;
}

void ElyanMI50::stop(IOService *provider)
{
    if (fRom)  { fRom->release();  fRom  = 0; }
    if (fBar0) { fBar0->release(); fBar0 = 0; }
    IOLog("ElyanMI50: stopped\n");
    super::stop(provider);
}
