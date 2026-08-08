/*
 * Automatically generated file - do not edit.
 */
#define DRIVER_NAME	lynxtwo
#define DRIVER_NICK	"lynxtwo"
#define DRIVER_PURPOSE	"lynxtwo"
#define DRIVER_STR_INFO	lynxtwo_str_info
#define DRIVER_ATTACH	lynxtwo_attach
#define DRIVER_DETACH	lynxtwo_detach
#define DRIVER_TYPE	DRV_PCI

int lynxtwo_init_order=0;
/*
 * lynxtwo_init_order determines if the output devices files are created before
 * the input ones (0) or vice versa (1).
 *
 * This may be necessary with a known application which expect all /dev/dsp#
 * devices to be recording devices.
 */
#include <linux/mod_devicetable.h>

#include <linux/pci_ids.h>

static struct pci_device_id id_table[] = {
	{.vendor=0x1621,	.device=0x20,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x21,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x22,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x23,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x24,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x25,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x28,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x29,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x20,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x21,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x22,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x23,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x24,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x25,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x28,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{.vendor=0x1621,	.device=0x29,	.subvendor=PCI_ANY_ID,	.subdevice=PCI_ANY_ID,	.class=PCI_CLASS_MULTIMEDIA_AUDIO},
	{0}
};

#include "module.inc"

module_param(lynxtwo_init_order, int, S_IRUGO);
MODULE_PARM_DESC(lynxtwo_init_order, 
"\n"
"lynxtwo_init_order determines if the output devices files are created before\n"
"the input ones (0) or vice versa (1).\n"
"\n"
"This may be necessary with a known application which expect all /dev/dsp#\n"
"devices to be recording devices.\n"
"\n");


