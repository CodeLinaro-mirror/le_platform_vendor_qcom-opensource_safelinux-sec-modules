M=$(PWD)

obj-m += drivers/tz_log.o
obj-m += drivers/qtee_shmbridge.o
obj-m += drivers/qcom_scm_oot.o
obj-m += drivers/smcinvoke.o

KERNEL_VERSION ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KERNEL_VERSION)/build

HDR_INSTAL_PATH ?= $(KDIR)/usr/include
INSTALL_HDR=$(shell mkdir -p $(HDR_INSTAL_PATH)/linux; \
			 cd $(KDIR);  \
			 $(KDIR)/scripts/headers_install.sh \
			 $(PWD)/drivers/smcinvoke.h $(HDR_INSTAL_PATH)/linux/smcinvoke.h; \
			 [ -f $(HDR_INSTAL_PATH)/linux/smcinvoke.h ] && echo "passed" || echo "failed")

modules:
		$(MAKE) -C "$(KDIR)" "M=$(M)" modules

modules_install:
		$(MAKE) -C "$(KDIR)" "M=$(M)"  modules_install

clean:
		$(MAKE) -C "$(KDIR)" "M=$(M)" clean

headers_install:
	 @echo "header installing $(INSTALL_HDR)"
