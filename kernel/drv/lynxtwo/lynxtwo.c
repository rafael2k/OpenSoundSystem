/*
 * Purpose: Driver for LynxTWO Studio Interface (family).
 */
#define COPYING Copyright (C) Hannu Savolainen and Dev Mazumdar 2003-2005. All rights reserved.

#include "lynxtwo_cfg.h"
#include <oss_pci.h>
#include "remux.h"
#include "lynxtwo.h"

extern int lynxhal_interrupt (lynxtwo_devc * devc);
extern int init_lynx_hal (lynxtwo_devc * devc);
extern int init_lynx_audio (lynxtwo_devc * devc);
extern int uninit_lynx_audio (lynxtwo_devc * devc);
extern int uninit_lynx_hal (lynxtwo_devc * devc);

#define HWINFO_SIZE	256

static int
lynxtwointr (oss_device_t * osdev)
{
  int serviced = 0;
  lynxtwo_devc *devc;

  devc = osdev->devc;

  if (lynxhal_interrupt (devc))
    {
      serviced = 1;
    }
  return serviced;
}

int
lynxtwo_attach (oss_device_t * osdev)
{
  unsigned char pci_revision /*, pci_latency */ ;
  unsigned short pci_command, vendor, device;
  unsigned int pci_addr0 = 0;
  unsigned int pci_addr1 = 0;
  int err;

  lynxtwo_devc *devc;

  pci_read_config_word (osdev, PCI_VENDOR_ID, &vendor);
  pci_read_config_word (osdev, PCI_DEVICE_ID, &device);

  if (vendor != LYNX_VENDOR_ID ||
      (device != LYNX_LYNXTWO_A &&
       device != LYNX_LYNXTWO_B &&
       device != LYNX_LYNXTWO_C && device != LYNX_LYNX_L22 &&
       device != LYNX_AES16 && device != LYNX_AES16_SRC &&
       device != LYNX_AES16e && device != LYNX_AES16e_SRC))
    return 0;

  pci_read_config_byte (osdev, PCI_REVISION_ID, &pci_revision);
  pci_read_config_word (osdev, PCI_COMMAND, &pci_command);
  pci_read_config_dword (osdev, PCI_MEM_BASE_ADDRESS_0, &pci_addr0);

  if (device != LYNX_AES16e && device != LYNX_AES16e_SRC)
    pci_read_config_dword (osdev, PCI_MEM_BASE_ADDRESS_1, &pci_addr1);

  if (pci_addr0 == 0)
    {
      cmn_err (CE_NOTE, "BAR0=0\n");
      return 0;
    }

  if ((devc = PMALLOC (osdev, sizeof (*devc))) == NULL)
    {
      cmn_err (CE_WARN, "Out of memory\n");
      return 0;
    }

  devc->osdev = osdev;
  osdev->devc = devc;

  osdev->hw_info = PMALLOC (osdev, HWINFO_SIZE);
  memset (osdev->hw_info, 0, HWINFO_SIZE);

  MUTEX_INIT (devc->osdev, devc->mutex, MH_DRV);
  devc->card_number = osdev->instance;

  devc->vendorID = vendor;
  devc->deviceID = device;

  devc->first_dev = -1;

  devc->bar0size = BAR0_SIZE;

  switch (device)
    {
    case LYNX_LYNXTWO_A:
      strcpy (devc->card_name, "LynxTWO-A");
      devc->model = MDL_LYNXTWO_A;
      break;
    case LYNX_LYNXTWO_B:
      strcpy (devc->card_name, "LynxTWO-B");
      devc->model = MDL_LYNXTWO_B;
      break;
    case LYNX_LYNXTWO_C:
      strcpy (devc->card_name, "LynxTWO-C");
      devc->model = MDL_LYNXTWO_C;
      break;
    case LYNX_LYNX_L22:
      strcpy (devc->card_name, "LynxL22");
      devc->model = MDL_L22;
      break;
    case LYNX_AES16:
      strcpy (devc->card_name, "Lynx AES16");
      devc->model = MDL_AES16;
      devc->bar0size = BAR0_SIZE_AES;
      break;
    case LYNX_AES16_SRC:
      strcpy (devc->card_name, "Lynx AES16-SRC");
      devc->model = MDL_AES16_SRC;
      devc->bar0size = BAR0_SIZE_AES;
      break;
    case LYNX_AES16e:
      strcpy (devc->card_name, "Lynx AES16e");
      devc->model = MDL_AES16;
      devc->bar0size = BAR0_SIZE_AESe;
      break;
    case LYNX_AES16e_SRC:
      strcpy (devc->card_name, "Lynx AES16e-SRC");
      devc->model = MDL_AES16_SRC;
      devc->bar0size = BAR0_SIZE_AESe;
      break;
    default:
      strcpy (devc->card_name, "LynxTWO (unknown hardware)");
      devc->bar0size = BAR0_SIZE_AES;	/* Assume AES16 */
    }

  if (pci_addr0)
    {
      devc->bar0addr = pci_addr0 & ~0xF;
      devc->bar0virt =
	(oss_uint64_t) MAP_PCI_MEM (devc->osdev, 0,
				       devc->bar0addr, devc->bar0size);
    }

  if (pci_addr1)
    {

      devc->bar1addr = pci_addr1 & ~0xF;
      devc->bar1virt =
	(oss_uint64_t) MAP_PCI_MEM (devc->osdev, 1,
				       devc->bar1addr, BAR1_SIZE);
    }

  /* activate the device */
  pci_command |= PCI_COMMAND_MASTER | PCI_COMMAND_MEMORY | PCI_COMMAND_IO;
  pci_write_config_word (devc->osdev, PCI_COMMAND, pci_command);

  oss_register_device (osdev, devc->card_name);

  if ((err = oss_register_interrupts (devc->osdev, 0, lynxtwointr, NULL)) < 0)
    {
      cmn_err (CE_WARN, "Can't register interrupt handler, err=%d\n", err);
      return 0;
    }

  if (!init_lynx_hal (devc))
    {
      cmn_err (CE_WARN, "HAL init failed\n");
      return 0;
    }

  sprintf (osdev->hw_info, "Firmware revision: %d PCB Revision %d\n",
	   PCI_READL (devc->osdev, devc->bar0virt + 0x10) & 0xffff,
	   PCI_READL (devc->osdev, devc->bar0virt + 0x0c) & 0xffff);

  if (!init_lynx_audio (devc))
    {
      cmn_err (CE_WARN, "Audio HAL init failed\n");
      return 0;
    }

#ifdef USE_REMUX
  {
    char tmp[80];

    sprintf (tmp, "%s 7.1 output", devc->card_name);
    remux_install (tmp, devc->osdev, devc->firstdev, devc->firstdev + 1,
		   devc->firstdev + 2, devc->firstdev + 3);
  }
#endif

  return 1;
}

int
lynxtwo_detach (oss_device_t * osdev)
{
  int err;
  lynxtwo_devc *devc = (lynxtwo_devc *) osdev->devc;

  if ((err = oss_disable_device (devc->osdev)) < 0)
    return 0;
  uninit_lynx_hal (devc);
  uninit_lynx_audio (devc);
  oss_unregister_interrupts (devc->osdev);
  MUTEX_CLEANUP (devc->mutex);

  if (devc->bar0addr)
    UNMAP_PCI_MEM (devc->osdev, 0, devc->bar0addr, (void *) devc->bar0virt,
		   devc->bar0size);

  if (devc->bar1addr)
    UNMAP_PCI_MEM (devc->osdev, 1, devc->bar1addr, (void *) devc->bar1virt,
		   BAR1_SIZE);

  oss_unregister_device (devc->osdev);

  return 1;
}
