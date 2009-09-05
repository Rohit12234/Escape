# general
BUILD=build
DISKMOUNT=disk
HDD=$(BUILD)/hd.img
VBHDDTMP=$(BUILD)/vbhd.bin
VBHDD=$(BUILD)/vbhd.vdi
HDDBAK=$(BUILD)/hd.img.bak
VMDISK=$(abspath vmware/vmwarehddimg.vmdk)
VBOXOSTITLE="Escape v0.1"
# 20 MB disk (40 * 16 * 63 * 512 = 20,643,840 byte)
HDDCYL=40
HDDHEADS=16
HDDTRACKSECS=63
TMPFILE=$(BUILD)/disktmp
BINNAME=kernel.bin
BIN=$(BUILD)/$(BINNAME)
SYMBOLS=$(BUILD)/kernel.symbols
OSTITLE=hrniels-OS

QEMUARGS=-serial stdio -hda $(HDD) -boot c -vga std

DIRS = tools libc libcpp services user kernel kernel/test

# flags for gcc
export CC = gcc
export CWFLAGS=-Wall -ansi \
				 -Wextra -Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-prototypes \
				 -Wmissing-declarations -Wnested-externs -Winline -Wno-long-long \
				 -Wstrict-prototypes -fno-builtin
export CPPWFLAGS=-Wall -Wextra -ansi \
				-Wshadow -Wpointer-arith -Wcast-align -Wwrite-strings -Wmissing-declarations \
				-Wno-long-long -fno-builtin
export CPPDEFFLAGS=$(CPPWFLAGS) -g -D DEBUGGING=1
export CDEFFLAGS=$(CWFLAGS) -g -D DEBUGGING=1
# flags for nasm
export ASMFLAGS=-f elf
# other
export SUDO=sudo

.PHONY: all mounthdd debughdd umounthdd createhdd dis qemu bochs debug debugu debugm debugt test clean

all: $(BUILD) $(DISKMOUNT)
		@[ -f $(HDD) ] || make createhdd;
		@for i in $(DIRS); do \
			make -C $$i all || { echo "Make: Error (`pwd`)"; exit 1; } ; \
		done

$(BUILD):
		[ -d $(BUILD) ] || mkdir -p $(BUILD);

$(DISKMOUNT):
		[ -d $(DISKMOUNT) ] || mkdir -p $(DISKMOUNT);

mounthdd: $(DISKMOUNT)
		@$(SUDO) umount $(DISKMOUNT) > /dev/null 2>&1 || true;
		@$(SUDO) mount -text2 -oloop=/dev/loop0,offset=`expr $(HDDTRACKSECS) \* 512` $(HDD) $(DISKMOUNT);

debughdd:
		$(SUDO) umount /dev/loop0 > /dev/null 2>&1 || true
		$(SUDO) losetup /dev/loop0 $(HDD) || true
		$(SUDO) fdisk /dev/loop0
		$(SUDO) umount /dev/loop0 || true
		$(SUDO) losetup -d /dev/loop0 || true

debugfs:
		make mounthdd;
		$(SUDO) debugfs /dev/loop0
		make umounthdd;

checkfs:
		make mounthdd;
		$(SUDO) fsck /dev/loop0 || true
		make umounthdd;

umounthdd:
		@tools/umounthdd.sh

createhdd: $(DISKMOUNT)
		$(SUDO) umount /dev/loop0 || true
		$(SUDO) losetup -d /dev/loop0 || true
		dd if=/dev/zero of=$(HDD) bs=`expr $(HDDTRACKSECS) \* $(HDDHEADS) \* 512`c count=$(HDDCYL)
		$(SUDO) losetup /dev/loop0 $(HDD) || true
		echo "n" > $(TMPFILE) && \
			echo "p" >> $(TMPFILE) && \
			echo "1" >> $(TMPFILE) && \
			echo "" >> $(TMPFILE) && \
			echo "20159" >> $(TMPFILE) && \
			echo "n" >> $(TMPFILE) && \
			echo "p" >> $(TMPFILE) && \
			echo "2" >> $(TMPFILE) && \
			echo "" >> $(TMPFILE) && \
			echo "" >> $(TMPFILE) && \
			echo "w" >> $(TMPFILE);
		$(SUDO) fdisk -u -C$(HDDCYL) -S$(HDDTRACKSECS) -H$(HDDHEADS) /dev/loop0 < $(TMPFILE) || true
		$(SUDO) losetup -d /dev/loop0
		$(SUDO) losetup -o`expr $(HDDTRACKSECS) \* 512` /dev/loop0 $(HDD)
		@# WE HAVE TO CHANGE THE BLOCK-COUNT HERE IF THE DISK-GEOMETRY OR PARTITION CHANGES!
		$(SUDO) mke2fs -r0 -Onone -b1024 /dev/loop0 10016
		$(SUDO) umount /dev/loop0 || true
		$(SUDO) losetup -d /dev/loop0 || true
		@# add boot stuff
		make mounthdd
		$(SUDO) mkdir $(DISKMOUNT)/boot
		$(SUDO) mkdir $(DISKMOUNT)/boot/grub
		$(SUDO) cp boot/stage1 $(DISKMOUNT)/boot/grub;
		$(SUDO) cp boot/stage2 $(DISKMOUNT)/boot/grub;
		$(SUDO) touch $(DISKMOUNT)/boot/grub/menu.lst;
		$(SUDO) chmod 0666 $(DISKMOUNT)/boot/grub/menu.lst;
		echo 'default 0' > $(DISKMOUNT)/boot/grub/menu.lst;
		echo 'timeout 0' >> $(DISKMOUNT)/boot/grub/menu.lst;
		echo '' >> $(DISKMOUNT)/boot/grub/menu.lst;
		echo "title $(OSTITLE)" >> $(DISKMOUNT)/boot/grub/menu.lst;
		echo "kernel /boot/$(BINNAME)" >> $(DISKMOUNT)/boot/grub/menu.lst;
		echo "module /sbin/ata /services/ata" >> $(DISKMOUNT)/boot/grub/menu.lst;
		echo "module /sbin/fs" /services/fs>> $(DISKMOUNT)/boot/grub/menu.lst;
		echo "boot" >> $(DISKMOUNT)/boot/grub/menu.lst;
		echo -n "device (hd0) $(HDD)\nroot (hd0,0)\nsetup (hd0)\nquit\n" | grub --no-floppy --batch;
		@# store some test-data on the disk
		$(SUDO) mkdir $(DISKMOUNT)/bin
		$(SUDO) mkdir $(DISKMOUNT)/sbin
		$(SUDO) mkdir $(DISKMOUNT)/etc
		$(SUDO) cp services/services.txt $(DISKMOUNT)/etc/services
		$(SUDO) mkdir $(DISKMOUNT)/testdir
		$(SUDO) touch $(DISKMOUNT)/file.txt
		$(SUDO) chmod 0666 $(DISKMOUNT)/file.txt
		echo "Das ist ein Test-String!!" > $(DISKMOUNT)/file.txt
		$(SUDO) cp user/test.bmp $(DISKMOUNT)
		$(SUDO) cp user/bbc.bmp $(DISKMOUNT)
		$(SUDO) cp user/test.bmp $(DISKMOUNT)/bla.bmp
		$(SUDO) cp $(DISKMOUNT)/file.txt $(DISKMOUNT)/testdir/file.txt
		$(SUDO) dd if=/dev/zero of=$(DISKMOUNT)/zeros bs=1024 count=1024
		$(SUDO) touch $(DISKMOUNT)/bigfile
		$(SUDO) chmod 0666 $(DISKMOUNT)/bigfile
		./tools/createStr.sh 'Das ist der %d Test\n' 200 > $(DISKMOUNT)/bigfile;
		make umounthdd
		rm -f $(TMPFILE)
		cp $(HDD) $(HDDBAK)
		@# first ensure that we'll copy all stuff to the disk with 'make all'
		rm -f $(BUILD)/*.bin
		touch services/services.txt
		@# now rebuild and copy it
		make all

$(VMDISK):	$(HDD)
		@qemu-img convert -f raw $(HDD) -O vmdk vmware/vmwarehddimg.vmdk

dis: all
		objdump -d -S $(BIN) | less

qemu:	all prepareRun
		qemu $(QEMUARGS) > log.txt 2>&1

bochs: all prepareRun
		bochs -f bochs.cfg -q | tee log.txt

vmware: all prepareRun $(VMDISK)
		vmplayer vmware/escape.vmx

vbox: all prepareRun $(VMDISK)
		tools/vboxhddupd.sh $(VBOXOSTITLE) $(VMDISK)
		VBoxSDL -startvm $(VBOXOSTITLE)

debug: all prepareRun
		qemu $(QEMUARGS) -S -s > log.txt 2>&1 &
		sleep 1;
		gdbtui --command=gdb.start --symbols $(BUILD)/kernel.bin

debugm: all prepareRun
		qemu $(QEMUARGS) -S -s > log.txt 2>&1 &

debugbochs: all prepareRun
		bochs -f bochs.cfg | tee log.txt

debugt: all prepareTest
		qemu $(QEMUARGS) -S -s > log.txt 2>&1 &

test: all prepareTest
		qemu $(QEMUARGS) > log.txt 2>&1

prepareTest: $(DISKMOUNT)
		make mounthdd
		@if [ "`cat $(DISKMOUNT)/boot/grub/menu.lst | grep kernel.bin`" != "" ]; then \
			$(SUDO) sed --in-place -e "s/^kernel.*/kernel \/boot\/kernel_test.bin/g" \
				$(DISKMOUNT)/boot/grub/menu.lst; \
				touch $(HDD); \
		fi;
		make umounthdd

prepareRun: $(DISKMOUNT)
		make mounthdd
		@if [ "`cat $(DISKMOUNT)/boot/grub/menu.lst | grep kernel_test.bin`" != "" ]; then \
			$(SUDO) sed --in-place -e "s/^kernel.*/kernel \/boot\/kernel.bin/g" \
				$(DISKMOUNT)/boot/grub/menu.lst; \
				touch $(HDD); \
		fi;
		make umounthdd

clean:
		@for i in $(DIRS); do \
			make -C $$i clean || { echo "Make: Error (`pwd`)"; exit 1; } ; \
		done
