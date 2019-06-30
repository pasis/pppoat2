LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE := pppoat
LOCAL_CFLAGS := -Wall -Werror
LOCAL_C_INCLUDES := $(LOCAL_PATH)/../src
LOCAL_SRC_FILES :=		\
	../src/base64.c		\
	../src/conf.c		\
	../src/conf_argv.c	\
	../src/conf_file.c	\
	../src/event.c		\
	../src/io.c		\
	../src/list.c		\
	../src/log.c		\
	../src/memory.c		\
	../src/misc.c		\
	../src/module.c		\
	../src/mutex.c		\
	../src/packet.c		\
	../src/queue.c		\
	../src/pipeline.c	\
	../src/sem.c		\
	../src/thread.c		\
	../src/pppoat.c		\
	../src/modules/if_fd.c	\
	../src/modules/if_pppd.c\
	../src/modules/if_tun.c	\
	../src/modules/tp_udp.c

include $(BUILD_EXECUTABLE)
