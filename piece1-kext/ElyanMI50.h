/* ElyanMI50 -- Darwin/PPC IOKit skeleton: attach to an AMD MI50 (Vega 20,
 * gfx906) over PCI, map its BARs, and dump the ATOM VBIOS. Piece 1 of a
 * compute kext for big-endian Mac OS X 10.5 on a Power Mac G5. MIT. */
#ifndef _ELYAN_MI50_H_
#define _ELYAN_MI50_H_
#include <IOKit/IOService.h>
#include <IOKit/pci/IOPCIDevice.h>

class ElyanMI50 : public IOService
{
    OSDeclareDefaultStructors(ElyanMI50)
public:
    virtual bool start(IOService *provider) override;
    virtual void stop(IOService *provider) override;
private:
    IOPCIDevice *fPci;
    IOMemoryMap *fBar0;   /* MMIO */
    IOMemoryMap *fRom;    /* expansion ROM = ATOM VBIOS */
};
#endif
