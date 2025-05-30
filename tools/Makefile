#             __________               __   ___.
#   Open      \______   \ ____   ____ |  | _\_ |__   _______  ___
#   Source     |       _//  _ \_/ ___\|  |/ /| __ \ /  _ \  \/  /
#   Jukebox    |    |   (  <_> )  \___|    < | \_\ (  <_> > <  <
#   Firmware   |____|_  /\____/ \___  >__|_ \|___  /\____/__/\_ \
#                     \/            \/     \/    \/            \/
#
CFLAGS := -Os -Wall -Wextra
LDFLAGS :=

.PHONY: rbspeexenc uclpack

CLEANALL := scramble descramble iriver bmp2rb rdf2binary convbdf \
    generate_rocklatin mkboot ipod_fw codepages uclpack mi4 gigabeat lngdump \
    telechips gigabeats creative hmac-sha1 rbspeexenc mkzenboot mk500boot \
    convttf mkspl-x1000

all: scramble descramble rdf2binary mkboot mkzenboot convbdf codepages \
	uclpack rbspeexenc voicefont mk500boot mkspl-x1000

scramble: scramble.o iriver.o mi4.o gigabeat.o gigabeats.o telechips.o \
	iaudio_bl_flash.o creative.o hmac-sha1.o rkw.o
descramble: descramble.o iriver.o gigabeat.o
scramble.o: scramble.c iriver.h mi4.h gigabeat.h telechips.h iaudio_bl_flash.h \
	creative.h rkw.h

descramble.o: descramble.c iriver.h gigabeat.h
creative.o: creative.c creative.h
hmac-sha1.o: hmac-sha1.c hmac-sha1.h
iriver.o: iriver.c iriver.h
gigabeat.o: gigabeat.c gigabeat.h
gigabeats.o: gigabeats.c gigabeats.h
mi4.o: mi4.c mi4.h
telechips.o: telechips.c telechips.h
rkw.o: rkw.c rkw.h
iaudio_bl_flash.o: iaudio_bl_flash.c iaudio_bl_flash.h
iaudio_bl_flash.c iaudio_bl_flash.h: iaudio_bl_flash.bmp bmp2rb
	$(SILENT)./bmp2rb -f 7 -h . $< >iaudio_bl_flash.c

bmp2rb:	bmp2rb.c
	$(SILENT)$(CC) -DAPPLICATION_NAME=\"$@\" $(CFLAGS) $+ -o $@

rdf2binary:	rdf2binary.c
	$(SILENT)$(CC) $(CFLAGS) $+ -o $@

mkboot:	mkboot.c
	$(SILENT)$(CC) $(CFLAGS) $+ -o $@

mk500boot: mk500boot.c mr500.c
	$(SILENT)$(CC) $(CFLAGS) $+ -o $@

mkzenboot.o: mkzenboot.c
	$(SILENT)$(CC) $(CFLAGS) -std=c99 -DSTANDALONE -c -o $@ $+

mkzenboot: mkzenboot.o hmac-sha1.o
	$(SILENT)$(CC) $(LDFLAGS) $+ -lz -o $@

mkspl-x1000: mkspl-x1000.c
	$(SILENT)$(CC) $(LDFLAGS) $+ -o $@

mkzenboot.exe: mkzenboot.o hmac-sha1.o
	$(SILENT)$(CC) $(LDFLAGS) -lz $+ C:\MingW\lib\libz.a -o $@

lngdump: lngdump.c
	$(SILENT)$(CC) $(CFLAGS) $+ -o $@

ipod_fw:	ipod_fw.c
	$(SILENT)$(CC) $(CFLAGS) $+ -o $@

checkwps: checkwps.c ../apps/gui/wps_parser.c ../apps/gui/wps_debug.c \
	../firmware/common/ctype.c ../apps/misc.c ../apps/recorder/bmp.c
	$(SILENT)$(CC) $(CFLAGS) -I ../apps/gui -I../firmware/export \
	    -D__PCTOOL__ -DDEBUG -DROCKBOX_HAS_LOGF -DIPOD_COLOR \
	    -D ROCKBOX_DIR_LEN -D WPS_DIR=\".\" -I../apps \
	    -I../firmware/target/arm/ipod -I../firmware/include $+ -o $@

convbdf: convbdf.c
	$(SILENT)$(CC) $(CFLAGS) $+ -o $@

codepages: codepages.c codepage_tables.c
	$(SILENT)$(CC) $(CFLAGS) $+ -o $@

uclpack:
	$(SILENT)$(MAKE) -C ucl

rbspeexenc:
	$(SILENT)$(MAKE) -C rbspeex

wavtrim: wavtrim.c
	$(SILENT)$(CC) $(CFLAGS) $+ -o $@

voicefont: voicefont.c
	$(SILENT)$(CC) $(CFLAGS) $+ -o $@

usb_benchmark: usb_benchmark.c
	$(SILENT)$(CC) $(CFLAGS) $+ -lusb -o $@

convttf: convttf.c
	$(call PRINTS,CC $(@F))
	$(SILENT)$(CC) $(CFLAGS) -lm -std=c99 -O2 -Wall -g $+ -o $@ \
		`pkg-config --cflags --libs freetype2`

clean:
	@echo "Cleaning tools"
	$(SILENT)rm -f $(CLEANALL) $(shell for f in $(CLEANALL) ; do \
	    echo $$f.exe $$f.o $$f.obj ; done) *.ajf *~
	$(SILENT)$(MAKE) -C ucl clean
	$(SILENT)$(MAKE) -C rbspeex clean

