#define VPX_CODEC_DISABLE_COMPAT 1

extern "C"
{
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
}
#include "PipeQueue.h"


#ifdef LOG_TAG
#undef LOG_TAG
#endif

#include "log.h"
#include <string.h>
#include <sys/time.h>
#include "vp8Encoder.h"
#include "vp8Decoder.h"

#define interface (vpx_codec_vp8_cx())

#define  LOG_TAG    "vp8_encoder"

//vp8Decoder *sDecoder;

vp8Encoder::vp8Encoder(int frameWidth, int frameHeight, IQueue *outChannel, pthread_mutex_t &lock)
    :PipeQueueHandler(0, "vp8Encoder"), mCodecLock(lock)
{
    display_width = frameWidth;
    display_height = frameHeight;
    mOutput = outChannel;
    //sDecoder = new vp8Decoder(frameWidth, frameHeight, 0, 0, 0);
}

vp8Encoder::~vp8Encoder()
{
    pthread_mutex_lock (&mCodecLock);
    LOGD("~vp8Encoder(%p)", this);
    pthread_mutex_unlock (&mCodecLock);
    //destory(); DONOT destory Encoder context before the thread stop.
    //destory() is called by super de-construct().
    //delete sDecoder;
}

int vp8Encoder::initilize(int frameWidth, int frameHeight)
{
    //display_width = frameWidth;
    //display_height = frameHeight;

    pthread_mutex_lock (&mCodecLock);
    frame_cnt = 0;

    vpx_codec_enc_cfg_t    cfg;
    int cpu_used = 8;
    int static_threshold = 1200;

    vpx_codec_enc_config_default(interface, &cfg, 0);
    LOGD("Using %s\n",vpx_codec_iface_name(interface));

    cfg.rc_target_bitrate = 10*frameWidth * frameHeight * cfg.rc_target_bitrate
        / cfg.g_w / cfg.g_h;
    LOGD("Encoder cfg.rc_target_bitrate=%d", cfg.rc_target_bitrate);
    cfg.g_w = display_width;
    cfg.g_h = display_height;
    /*cfg.g_timebase.num = 1;
    cfg.g_timebase.den = (int) 10000000;
    cfg.rc_end_usage = VPX_CBR;
    cfg.g_pass = VPX_RC_ONE_PASS;
    cfg.g_lag_in_frames = 0;
    cfg.rc_min_quantizer = 20;
    cfg.rc_max_quantizer = 50;
    cfg.rc_dropframe_thresh = 1;
    cfg.rc_buf_optimal_sz = 1000;
    cfg.rc_buf_initial_sz = 1000;
    cfg.rc_buf_sz = 1000;
    cfg.g_error_resilient = 1;
    cfg.kf_mode = VPX_KF_DISABLED;
    cfg.kf_max_dist = 999999;
    cfg.g_threads = 1;*/

    vpx_codec_enc_init(&encoder, interface, &cfg, 0);
    /*vpx_codec_control_(&encoder, VP8E_SET_CPUUSED, cpu_used);
    vpx_codec_control_(&encoder, VP8E_SET_STATIC_THRESHOLD, static_threshold);
    vpx_codec_control_(&encoder, VP8E_SET_ENABLEAUTOALTREF, 0);
    */
    vpx_img_alloc(&raw, VPX_IMG_FMT_I420, display_width, display_height, 1);

    //create_packetizer(&x, XOR, fec_numerator, fec_denominator);

    //sDecoder->initilize();
    pthread_mutex_unlock (&mCodecLock);
    return 0;
}

int vp8Encoder::encode(int buffer_time, uint8_t* video_data)
{
    struct timeval start;
    struct timeval end;

    gettimeofday(&start, NULL);
    uint8_t *src = video_data;

    int yPlaneSize = raw.d_w * raw.d_h;

    memcpy(raw.planes[VPX_PLANE_Y], src, yPlaneSize);

    unsigned char *inPtr = src + yPlaneSize;
    unsigned char *cbPtr = raw.planes[VPX_PLANE_U];
    unsigned char *crPtr = raw.planes[VPX_PLANE_V];

    for(int i = 0; i < raw.d_h/2; i++)
    {
        for(int j = 0; j < raw.d_w/2; j++)
        {
            cbPtr[j] = inPtr[0];
            crPtr[j] = inPtr[1];
            inPtr+=2;
        }
        cbPtr += raw.stride[VPX_PLANE_U];
        crPtr += raw.stride[VPX_PLANE_V];
    }

    const vpx_codec_cx_pkt_t *pkt;
    vpx_codec_iter_t iter = NULL;
    int flags = 0;

    if(!(frame_cnt & 3))                                              //
        flags |= VPX_EFLAG_FORCE_KF;                                  //
    else                                                              //
        flags &= ~VPX_EFLAG_FORCE_KF;

    //pthread_mutex_lock (&mCodecLock);
    LOGD("Start to encoding ...");
    vpx_codec_encode(&encoder, &raw, frame_cnt++, 1, flags, 20000);//VPX_DL_REALTIME);

    while ((pkt = vpx_codec_get_cx_data(&encoder, &iter)))
    {
        if (pkt->kind == VPX_CODEC_CX_FRAME_PKT)
        {
            gettimeofday(&end, NULL);
            long time_use = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
            LOGD("vp8encode consume=%.2fms.", (double)time_use/1000.0);

            mOutput->appendData((uint8_t*)pkt->data.frame.buf, pkt->data.frame.sz);
        }
    }
    //pthread_mutex_unlock (&mCodecLock);
}

void vp8Encoder::destory()
{
    LOGD("vp8Encoder::destory()");
    vpx_codec_destroy(&encoder);
    vpx_img_free(&raw);
}




