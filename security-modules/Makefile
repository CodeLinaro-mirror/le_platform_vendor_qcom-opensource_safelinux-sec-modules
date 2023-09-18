M=$(PWD)

obj-m += drivers/tz_log.o
obj-m += drivers/qtee_shmbridge.o
obj-m += drivers/qcom_scm_oot.o
obj-m += drivers/smcinvoke.o

KERNEL_VERSION ?= $(shell uname -r)
KDIR ?= /lib/modules/$(KERNEL_VERSION)/build

modules:
		$(MAKE) -C "$(KDIR)" "M=$(M)" modules

modules_install:
		$(MAKE) -C "$(KDIR)" "M=$(M)"  modules_install

clean:
		$(MAKE) -C "$(KDIR)" "M=$(M)" clean

