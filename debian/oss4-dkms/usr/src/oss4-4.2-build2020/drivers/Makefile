osscore_symbols.inc:
	echo "static const struct modversion_info ____versions[]" >> osscore_symbols.inc
	echo " __attribute__((used))" >> osscore_symbols.inc
	echo "__attribute__((section(\"__versions\"))) = {" >> osscore_symbols.inc
	sed -e "s:^:{:" -e "s:\t:, \":" -e "s:\t\(.\)*:\"},:" < $(PWD)/core/Module.symvers >> osscore_symbols.inc
	echo "};" >> osscore_symbols.inc

KBUILD_EXTRA_SYMBOLS := $(PWD)/core/Module.symvers

MULTIARCH_PATH = /usr/include/$(shell dpkg-architecture -qDEB_HOST_MULTIARCH)
EXTRA_CFLAGS=-D_KERNEL -I$(KBUILD_EXTMOD)/../core -I$(KBUILD_EXTMOD) -isystem /usr/include -isystem $(MULTIARCH_PATH) 	-isystem $(shell $(CC) --print-file-name=include-fixed)
obj-m += lynxone.o
lynxone-objs := lynx_hal.o lynxonedriver.o lynxonemodule.o
obj-m += oss_ali5455.o
oss_ali5455-objs := oss_ali5455driver.o oss_ali5455module.o
obj-m += oss_atiaudio.o
oss_atiaudio-objs := oss_atiaudiodriver.o oss_atiaudiomodule.o
obj-m += oss_audigyls.o
oss_audigyls-objs := oss_audigylsdriver.o oss_audigylsmodule.o
obj-m += oss_audioloop.o
oss_audioloop-objs := oss_audioloopdriver.o oss_audioloopmodule.o
obj-m += oss_audiopci.o
oss_audiopci-objs := oss_audiopcidriver.o oss_audiopcimodule.o
obj-m += oss_cmi878x.o
oss_cmi878x-objs := oss_cmi878xdriver.o oss_cmi878xmodule.o
obj-m += oss_cmpci.o
oss_cmpci-objs := oss_cmpcidriver.o oss_cmpcimodule.o
obj-m += oss_cs4281.o
oss_cs4281-objs := oss_cs4281driver.o oss_cs4281module.o
obj-m += oss_cs461x.o
oss_cs461x-objs := oss_cs461xdriver.o oss_cs461xmodule.o
obj-m += oss_digi96.o
oss_digi96-objs := oss_digi96driver.o oss_digi96module.o
obj-m += oss_emu10k1x.o
oss_emu10k1x-objs := oss_emu10k1xdriver.o oss_emu10k1xmodule.o
obj-m += oss_envy24.o
oss_envy24-objs := envy24_1010lt.o envy24_6fire.o envy24_default.o envy24_direct.o envy24_ews88d.o envy24_tdif.o oss_envy24driver.o oss_envy24module.o
obj-m += oss_envy24ht.o
oss_envy24ht-objs := envy24ht_ac97.o envy24ht_ap192.o envy24ht_aureon.o envy24ht_julia.o envy24ht_revo51.o envy24ht_revo71.o oss_envy24htdriver.o oss_envy24htmodule.o
obj-m += oss_fmedia.o
oss_fmedia-objs := oss_fmediadriver.o oss_fmediamodule.o
obj-m += oss_geode.o
oss_geode-objs := oss_geodedriver.o oss_geodemodule.o
obj-m += oss_hdaudio.o
oss_hdaudio-objs := hdaudio_abit_AA8.o hdaudio_asus_m9.o hdaudio_asus_P4B_E.o hdaudio_codec.o hdaudio_eeepc.o hdaudio_ferrari5k.o hdaudio_generic.o hdaudio_gpio_handlers.o hdaudio_scaleoP.o hdaudio_si3055.o hdaudio_thinkpad_r61.o hdaudio_vaio_vgn.o oss_hdaudiodriver.o oss_hdaudiomodule.o
obj-m += oss_ich.o
oss_ich-objs := oss_ichdriver.o oss_ichmodule.o
obj-m += oss_imux.o
oss_imux-objs := oss_imuxdriver.o oss_imuxmodule.o
obj-m += oss_madi.o
oss_madi-objs := madi_mixer.o oss_madidriver.o oss_madimodule.o
obj-m += oss_midiloop.o
oss_midiloop-objs := oss_midiloopdriver.o oss_midiloopmodule.o
obj-m += oss_midimix.o
oss_midimix-objs := oss_midimixdriver.o oss_midimixmodule.o
obj-m += oss_sblive.o
oss_sblive-objs := oss_sblivedriver.o oss_sblivemodule.o
obj-m += oss_sbpci.o
oss_sbpci-objs := oss_sbpcidriver.o oss_sbpcimodule.o
obj-m += oss_sbxfi.o
oss_sbxfi-objs := oss_sbxfidriver.o sbxfi_hwaccess.o oss_sbxfimodule.o
obj-m += oss_solo.o
oss_solo-objs := oss_solodriver.o oss_solomodule.o
obj-m += oss_trident.o
oss_trident-objs := oss_tridentdriver.o oss_tridentmodule.o
obj-m += oss_usb.o
oss_usb-objs := ossusb_audio.o oss_usbdriver.o ossusb_midi.o ossusb_midisport.o ossusb_ymhmidi.o oss_usbmodule.o
obj-m += oss_userdev.o
oss_userdev-objs := oss_userdevdriver.o oss_userdev_devicepair.o oss_userdevmodule.o
obj-m += oss_via823x.o
oss_via823x-objs := oss_via823xdriver.o oss_via823xmodule.o
obj-m += oss_via97.o
oss_via97-objs := oss_via97driver.o oss_via97module.o
obj-m += oss_ymf7xx.o
oss_ymf7xx-objs := oss_ymf7xxdriver.o oss_ymf7xxmodule.o
