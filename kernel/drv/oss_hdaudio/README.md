oss_hdaudio: Intel HD Audio controller and Realtek ALC298 codec support
=========================================================================

This documents the controller and codec support added to oss_hdaudio for
modern Intel platforms, the fixes required to make it work reliably under
current Linux kernels, and the driver's OSS 3.6 legacy mixer compatibility
layer.

Tested and verified working on: Lenovo ThinkPad P70 (Intel HD Audio
controller, PCI ID 8086:a170, "SKL" in this driver's naming; Realtek
ALC298 codec, HDA vendor ID 0x10ec0298), Debian 13, kernel 6.12.101.
None of the changes below are specific to that one laptop -- the
controller ID table and DMA fixes apply to any sufficiently modern Intel
HDA platform, and the ALC298 codec ID is shared by many other designs.

Controller support added
---------------------------

The controller's PCI ID table (devlists/Linux, kernel/drv/oss_hdaudio/
.devices, and the attach switch in oss_hdaudio.c) previously stopped at
older chipset generations. Intel controller IDs were added for:

  HSW, BDW, BYT, BSW, SKL(+LP), SPT(+LP), KBL(+H), CNL(+LP/H),
  CML(+LP/H/S), ICL(+LP/N/H), TGL(+LP/H), ADL(+P/PX/M/N/S), RPL-S,
  APL, GML

covering Intel HD Audio controllers from Haswell/Baytrail-era hardware
through Alder Lake/Raptor Lake.

Codec support added
-----------------------

Realtek ALC298 (ALC269 family) was added to the codec table, with
widget remap names for the pin/jack layout used on ThinkPad-style
designs -- kernel/drv/oss_hdaudio/hdaudio_codecids.h.

Reliability fixes
---------------------

A number of issues prevented the controller/codec combination above
from working at all on a modern kernel, independent of the ID table
additions:

  * DMA allocation used __get_free_pages()/virt_to_phys(), which does
    not produce addresses valid for a device behind an IOMMU. On an
    IOMMU-enabled system this caused DMAR faults as soon as the
    controller's CORB/RIRB command/response buffers were touched.
    Fixed by new DMA helpers in kernel/OS/Linux/os_linux.c
    (oss_dma_alloc(), oss_dma_free(), oss_dma_capable(), oss_dma_has())
    that use dma_alloc_attrs()/dma_free_attrs() and keep a va->dma
    mapping table so a buffer can be freed correctly regardless of
    which osdev pointer (or NULL, for vmix subdevices) is passed to
    free.

  * ALSA's snd_hda_intel, if it had probed the card earlier (e.g. an
    earlier boot, or a coexisting kernel module), can leave the codec
    powered down in D3. In that state the codec answers every verb
    with 0x00000000, including the vendor-ID read used to identify it.
    attach_codec() now retries the vendor-ID read like ALSA does, and
    falls back to an explicit SET_CODEC_RESET + re-power + 250 ms wait
    (per the HDA spec) if the codec isn't answering.

  * vmix allocates its virtual sub-device DMA buffers through the
    parent PCI device's DMA-API path but previously freed them through
    a different, NULL-osdev fallback path meant for non-PCI (page
    based) buffers, causing a crash on close. Fixed by having the
    DMA-buffer tracking table record the allocating device, so a free
    is routed correctly regardless of which osdev is passed to it.

  * Verbose RIRB entry trace, controller state dump and vendor-probe
    tracing were unconditionally printed to dmesg on every attach.
    Kept in the source under #if 0 for future debugging, but silenced
    by default.

Legacy OSS 3.6 mixer compatibility
------------------------------------

Historically, oss_hdaudio's hda_mixer_ioctl() unconditionally reported
an empty legacy device mask, so any application using the old OSS 3.6
SOUND_MIXER_* ioctl API (rexima being the canonical example -- it does
not speak the modern mixer_ext/SNDCTL_MIX_* API at all) saw "mixer has
no devices", even though the card works fine through ossmix/ossxmix.

HDA codec topology (which widgets exist, how they're named) varies too
much across machines to hardcode a fixed legacy control set the way
e.g. oss_envy24 does for its fixed hardware. Instead, hda_mixer_ioctl()
locates a small set of legacy slots dynamically, by pattern-matching
this specific mixer device's own already-created mixer_ext controls:

  SOUND_MIXER_VOLUME, SOUND_MIXER_PCM  -> the "vmix<N>-outvol" control
                                          (master output level; vmix's
                                          instance number is looked up
                                          rather than assumed to be 0,
                                          so this also works if this
                                          isn't the first sound card)
  SOUND_MIXER_MIC                      -> "misc.mic" if present, else
                                          "jack.int-mic"

DEVMASK/RECMASK/RECSRC/STEREODEVS are computed from what's *actually*
found on this card, not a fixed assumption -- if a given machine's ALC298
(or a different codec entirely) doesn't expose a control matching one of
the patterns above, that slot is simply absent from the mask rather than
present-but-broken. Values are read/written through the control's own
mixer_ext handler function (the same one ossmix/ossxmix use), converted
between its native 0..maxvalue range and the legacy 0-100 range -- not
via a separate, possibly-inconsistent code path.

Known limitations
--------------------

  * Only VOLUME/PCM (mapped together, to the single master output
    level) and MIC are implemented in the legacy mixer layer. Legacy
    apps that expect independent LINE, CD, or per-jack controls won't
    find them -- HDA's per-jack, per-path mixer model doesn't map
    cleanly onto OSS 3.6's fixed 25-slot model, and 4Front's original
    driver never attempted this mapping at all (hda_mixer_ioctl()
    always returned an empty mask upstream). The mapping here is
    intentionally conservative rather than guessing at additional
    slots.

  * mmap() on /dev/dsp* will fail with "mmap() not possible with
    currently selected sample format" whenever the stream is in
    "cooked" mode (format/rate conversion, which is essentially always
    the case once vmix or PulseAudio's software mixing is involved).
    This is generic OSS4 core behavior (kernel/OS/Linux/os_linux.c and
    kernel/OS/FreeBSD/os_freebsd.c both have the identical check), not
    specific to this driver -- mmap requires direct zero-copy access to
    the hardware buffer, which is incompatible with a software
    conversion step in between.

  * On at least the platform this was verified against, unloading and
    reloading osscore/oss_hdaudio at runtime does not reliably bring
    the codec back (CORB/RIRB verb transactions time out on reload,
    even though the driver's own controller-level reset succeeds).
    This appears to be related to Intel's audio/display power-well
    coupling on integrated-graphics platforms (ALSA's snd_hda_intel
    explicitly binds to i915's "audio component" framework to hold the
    relevant power well open for as long as it's loaded; oss_hdaudio
    does not participate in that framework at all). A full reboot
    reliably recovers the device; a live module reload may not.
    Implementing i915 audio-component binding to fix this would be a
    separate, substantial piece of work.

Building/installing
----------------------

The standalone `./configure && make` build never compiles Linux kernel
drivers correctly on modern kernels (it doesn't use real kbuild and
never points at real kernel headers -- see debian/create-ma-tree.sh for
the build path that does work). To build and install this driver:

  bash ./debian/create-ma-tree.sh build-tree/modules/oss4 build-tree/oss-build
  # (after the usual ./configure + `make build` step in build-tree/oss-build,
  #  see debian/rules for the exact sequence)

or, for a normal end-user install, via the Debian package's DKMS
integration (`dpkg -i oss4-dkms_*.deb`, or `dkms build`/`dkms install`
against an existing /usr/src/oss4-<version>/ tree) -- this is what
actually builds and signs the modules that get loaded at boot.
