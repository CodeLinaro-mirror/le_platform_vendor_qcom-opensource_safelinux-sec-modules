KDIR ?= $(KERNEL_SRC)
SECURITY_MODULES_ROOT=$(M)
KBUILD_OPTIONS += SECURITY_MODULES_ROOT=$(SECURITY_MODULES_ROOT)

HDR_INSTAL_PATH ?= $(KDIR)/usr/include
INSTALL_HDR=$(shell mkdir -p $(HDR_INSTAL_PATH)/linux; \
			 cd $(KDIR);  \
			 $(KDIR)/scripts/headers_install.sh \
			 $(PWD)/drivers/smcinvoke.h $(HDR_INSTAL_PATH)/linux/smcinvoke.h; \
			 [ -f $(HDR_INSTAL_PATH)/linux/smcinvoke.h ] && echo "passed" || echo "failed")

all: clean modules

modules:
		$(MAKE) -C $(KDIR) M=$(M) $(KBUILD_OPTIONS) modules

modules_install:
		$(MAKE) -C $(KDIR) M=$(M) modules_install

clean:
		$(MAKE) -C $(KDIR) M=$(M) clean

headers_install:
	 @echo "header installing $(INSTALL_HDR)"
