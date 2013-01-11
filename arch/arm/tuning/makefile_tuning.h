#
# Makefile for the linux kernel.
#

ifeq ($(CONFIG_MACH_STAR),y)
KBUILD_CFLAGS  += $(call cc-option,-O3)
KBUILD_CFLAGS  += $(call cc-option,-fsingle-precision-constant)
KBUILD_CFLAGS  += $(call cc-option,-mfloat-abi=softfp,-msoft-float)
KBUILD_CFLAGS  += $(call cc-option,-mfpu=vfpv3-d16)

KBUILD_AFLAGS  += $(call as-option,-Wa$(comma)-mfloat-abi=softfp,-Wa$(comma)-msoft-float)
KBUILD_AFLAGS  += $(call as-option,-Wa$(comma)-mfpu=vfpv3-d16)

tune-y         += $(call cc-option,-mtune=cortex-a9)
tune-y         += $(call cc-option,-mcpu=cortex-a9)
tune-y         += $(call as-option,-Wa$(comma)-march=armv7-a)
tune-y         += $(call as-option,-Wa$(comma)-mcpu=cortex-a9)
endif # CONFIG_MACH_STAR
