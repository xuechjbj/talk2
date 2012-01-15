LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
ANDR_ROOT_DIR:=/home/xuechaojun/mywork/andr-src/stable5
#ANDR_ROOT_DIR:=/home/xuechaojun/andr_src/stable5
LIBEVENT_ROOT_DIR:=/home/xuechaojun/Downloads/libevent-2.0.12-stable

LOCAL_SRC_FILES := listening_server.cpp vp8Encoder.cpp  vp8Decoder.cpp
#vp8coder_jni.cpp  
#rpc.cpp rpc_cmds.gen.c 

LOCAL_C_INCLUDES += ${LIBEVENT_ROOT_DIR}/include \
                    ${ANDR_ROOT_DIR}/external/libvpx \
					${ANDR_ROOT_DIR}/frameworks/base/include \
					${ANDR_ROOT_DIR}/system/core/include \
					${ANDR_ROOT_DIR}/external/skia/include/core \
					${ANDR_ROOT_DIR}/external/skia/include/effects \
					${ANDR_ROOT_DIR}/external/skia/include/images \
					${ANDR_ROOT_DIR}/external/skia/src/ports \
					${ANDR_ROOT_DIR}/external/skia/include/utils

LOCAL_CFLAGS += -g
LOCAL_LDLIBS :=  -L./ -L../ \
                 -L${ANDR_ROOT_DIR}/out/target/product/cdma_solana-p1a_solana/symbols/system/lib \
				 -levent -levent_pthreads -llog -lvpx -lskia

LOCAL_MODULE := talk2jni

include $(BUILD_SHARED_LIBRARY)
