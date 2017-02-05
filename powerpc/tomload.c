#include <stdint.h>
#include <stddef.h>

#include "mmio.h"
#include "tlb.h"
#include "pci.h"
#include "pci_def.h"
#include "common.h"

#define NELEMENTS(ARR) (sizeof(ARR)/sizeof(ARR[0]))

void load_os(uint32_t); /* in init-tom.S */

static const
tlbentry initial_mappings[] = {
	/* leave initial ROM and RAM mappings in place (first 3) */

	/* CCSR (after relocation) */
	{.mas0 = MAS0_TLB1,
	 .mas1 = MAS1_V|MAS1_TSIZE(0x5), /* 1 MB */
	 .mas2 = MAS2_EPN(0xe1000000)|MAS2_DEVICE,
	 .mas3 = MAS3_RPN(0xe1000000)|MAS3_DEVICE,
	},

	/* mvme3100 CPLD registers */
	{.mas0 = MAS0_TLB1,
	 .mas1 = MAS1_V|MAS1_TSIZE(0x7), /* 16 MB */
	 .mas2 = MAS2_EPN(0xe2000000)|MAS2_DEVICE,
	 .mas3 = MAS3_RPN(0xe2000000)|MAS3_DEVICE,
	},

	/* PCI IO ports (cf. setup_pci_host()) */
	{.mas0 = MAS0_TLB1,
	 .mas1 = MAS1_V|MAS1_TSIZE(0x7), /* 16 MB */
	 .mas2 = MAS2_EPN(0xe0000000)|MAS2_DEVICE,
	 .mas3 = MAS3_RPN(0xe0000000)|MAS3_DEVICE,
	},

	/* PCI (cf. setup_pci_host()) */
	{.mas0 = MAS0_TLB1,
	 .mas1 = MAS1_V|MAS1_TSIZE(0x9), /* 256 MB */
	 .mas2 = MAS2_EPN(0x80000000)|MAS2_DEVICE,
	 .mas3 = MAS3_RPN(0x80000000)|MAS3_DEVICE,
	},

	/* PCI (cf. setup_pci_host()) */
	{.mas0 = MAS0_TLB1,
	 .mas1 = MAS1_V|MAS1_TSIZE(0x9), /* 256 MB */
	 .mas2 = MAS2_EPN(0xc0000000)|MAS2_DEVICE,
	 .mas3 = MAS3_RPN(0xc0000000)|MAS3_DEVICE,
	},
};

static
void setup_tlb(void)
{
	tlbentry disabled;
	unsigned idx, n;
	/* Leave first 3 mappings (ROM and RAM) in place,
	 * copy in remaining
	 */
	for(idx=0, n=3; idx<NELEMENTS(initial_mappings); idx++, n++)
	{
		tlbentry ent = initial_mappings[idx];
		ent.mas0 |= MAS0_ENT(n);
		tlb_update(&ent);
	}
	/* disable remaining entries */
	disabled.mas0 = disabled.mas1 = disabled.mas2 = disabled.mas3 = 0;
	for(;n<16;n++)
	{
		disabled.mas0 |= MAS0_ENT(n);
		tlb_update(&disabled);
	}
}

static
void setup_i2c()
{
    /* MOTLOAD leaves the controller enabled,
     * and RTEMS blindly assumes this is the case
     */
    out8x(CCSRBASE, 0x3008, 0x80);
}

/* for PCI address assignment */
static
uint32_t next_mmio = 0x80000000,
         next_io   = 0xe0000000;

/* prepare the PCI host bridge */
static
void setup_pci_host(void)
{
	/* Setup MMIO window 256 MB */
	out32x(CCSRBASE, 0x8c20, next_mmio>>12);
	out32x(CCSRBASE, 0x8c24, 0x00000000);
	out32x(CCSRBASE, 0x8c28, next_mmio>>12);
	out32x(CCSRBASE, 0x8c30, 0x8004401b);
	/* Setup IO port window 16 MB */
	out32x(CCSRBASE, 0x8c40, next_io>>12);
	out32x(CCSRBASE, 0x8c44, 0x00000000);
	out32x(CCSRBASE, 0x8c48, next_io>>12);
	out32x(CCSRBASE, 0x8c50, 0x80088017);
	/* Extra window used by tsi148, 256 MB */
	out32x(CCSRBASE, 0x8c60, 0xc0000000>>12);
	out32x(CCSRBASE, 0x8c64, 0x00000000);
	out32x(CCSRBASE, 0x8c68, 0xc0000000>>12);
	out32x(CCSRBASE, 0x8c70, 0x8004401b);
	/* TODO: setup inbound window(s) to support DMA */
}

static
void map_pci_interrupt(PCIDevice *dev)
{
    /* Set IRQ lines
     *  A mvme3100 has (at least) three PCI busses
     * Bus 0 (A) has only two devices w/ IRQ, and an arbitrary IRQ assignment
     * slot 17 (tsi148)
     *   IRQ0, IRQ1, IRQ2, IRQ3
     * slot 20 (sata0
     *   IRQ2
     * Bus 1 (B) and 2 (C) have the conventional rotatin assignments
     *  Bus 1
     *    slot 0 IRQ4, IRQ5, IRQ6, IRQ7
     *    slot 1 IRQ5, IRQ6, IRQ7, IRQ4
     *  Bus 2 slot 0
     *   IRQ4, IRQ5, IRQ6
     *   ...
     */
    uint8_t pin = pci_in8(dev->B, dev->D, dev->F, PCI_INTERRUPT_PIN),
            line = 0;
    if(dev->B==0) {
        if(0) {}
        else if(dev->D==0x11 && dev->F==0 && pin) line = 0;
        else if(dev->D==0x12 && dev->F==0 && pin) line = 4;
    } else if(pin && (dev->B==1 || dev->B==2)) {
        line = (pin-1u+dev->D)&3u;
        line += 4u;
    }
    
    pci_out8(dev->B, dev->D, dev->F, PCI_INTERRUPT_LINE, line);
}

typedef struct {
	unsigned vendor, device, subvendor, subdevice, devclass;
} pciid;

static
int pci_find(const pciid *id, unsigned index, unsigned *rb, unsigned *rd, unsigned *rf)
{
	unsigned d;
	for(d=0; d<0x20; d++) {
		uint32_t val = pci_in16(0,d,0,0);
		if(val==0xffff)
			continue;
		if(d!=0x11)
			continue;
		if(id->vendor!=(unsigned)-1 && val!=id->vendor)
			continue;
		val = pci_in16(0,d,0,2);
		if(id->device!=(unsigned)-1 && val!=id->device)
			continue;
		val = pci_in16(0,d,0,0x2c);
		if(id->subvendor!=(unsigned)-1 && val!=id->subvendor)
			continue;
		val = pci_in16(0,d,0,0x2e);
		if(id->subdevice!=(unsigned)-1 && val!=id->subdevice)
			continue;
		if(index!=0) {
			index--;
			continue;
		}
		*rb = 0;
		*rd = d;
		*rf = 0;
		return 0;
	}
	return -1;
}

static
void prepare_tsi148(void)
{
	uint32_t bar;
	pciid tsi148 = {
		.vendor = 0x10e3,
		.device = 0x0148,
		.subvendor = -1,
		.subdevice = -1,
		.devclass = -1,
	};
	unsigned b, d, f;
	if(pci_find(&tsi148, 0, &b,&d,&f))
		return;
	bar = pci_in32(b,d,f,0x10)&0xffffff00;
	if(!bar)
		return;
	out32x(bar, 0x604, 1<<27); /* master enable on */
}

/* on entry only ROM, RAM, and CCSR are accessible */
void Init(void)
{
	uint16_t vend = pci_in16(0,0,0,0),
	         device = pci_in16(0,0,0,2);
	if(vend!=0x1957 || device!=0x30)
		return; /* wrong host bridge, something is really wrong here */

	setup_tlb(); /* PCI memory regions now accessible */
    printk("TOMLOAD\n");

    setup_pci_host();

    {
        PCIBus *host = pci_alloc_host_bus();
        host->mmio_next = 0x80000000;
        host->io_next   = 0xe0000000;
        host->irqfn     = &map_pci_interrupt;
        if(pci_scan_bus(host)) {
            printk("PCI Scan error\n");
        } else {
            pci_show_bus(host);
        }
        pci_setup_bus(host);
    }
    
	setup_i2c();
	//walk_bus(0);
	prepare_tsi148();
	load_os(0x10000);
}
