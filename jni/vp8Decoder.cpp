#define VPX_CODEC_DISABLE_COMPAT 1

#include "vp8Decoder.h"

#define LOG_TAG "vp8Decoder"
#define interface (vpx_codec_vp8_dx())

vp8Decoder::vp8Decoder(int frameWidth, int frameHeight, pthread_mutex_t &lock)
    :mCodecLock(lock)
{
    display_width =  frameWidth;
    display_height = frameHeight;
    mRGBBufSize = sizeof(__u16) * display_width * display_height;
    mrgbbuffer = (__u16*)malloc(mRGBBufSize);
    index = 0;
};

vp8Decoder::~vp8Decoder()
{
    pthread_mutex_lock (&mCodecLock);
    LOGD("~vp8Decoder(%p)", this);
    pthread_mutex_unlock (&mCodecLock);
    //destory(); DONOT destory Decoder context before the thread stop.
    //destory() is called by super de-construct().
}

int vp8Decoder::initilize()
{
    pthread_mutex_lock (&mCodecLock);
    vpx_codec_dec_init(&decoder, interface, &cfg, 0);
    pthread_mutex_unlock (&mCodecLock);

    //buf = (uint8_t *) malloc(display_width * display_height * 3 / 2);

    /* Config post processing settings for decoder */
    /*ppcfg.post_proc_flag = VP8_DEMACROBLOCK | VP8_DEBLOCK | VP8_ADDNOISE;
    ppcfg.deblocking_level = 5   ;
    ppcfg.noise_level = 1  ;
    vpx_codec_control(&decoder, VP8_SET_POSTPROC, &ppcfg);*/
    return 0;
}

void vp8Decoder::destory()
{
    LOGD("vp8Decoder::destory()");
    if (vpx_codec_destroy(&decoder))
    {
        LOGE("Failed to destroy decoder: %s\n", vpx_codec_error(&decoder));

    }
    free(mrgbbuffer);
}

int vp8Decoder::decode(uint8_t *data, int length, struct DecodeData *d)
{
    if(length <= 0) return -1;

    struct timeval start;
    struct timeval end;

    gettimeofday(&start, NULL);

    vpx_codec_iter_t  iter = NULL;
    vpx_image_t    *img;

    //pthread_mutex_lock (&mCodecLock);
    LOGD("vp8Decode get data length=%d, start decoding", length);
    if (vpx_codec_decode(&decoder, data, length, 0, 0))
    {
        LOGE("Failed to decode frame: %s\n", vpx_codec_error(&decoder));
        const char *detail = vpx_codec_error_detail(&decoder);
        if(detail)
        {
            LOGE("%s", detail);
        }
        pthread_mutex_unlock (&mCodecLock);
        return -1;
    }

    img = vpx_codec_get_frame(&decoder, &iter);
    LOGD("decode fmt=%d, w=%d, h=%d", img->fmt, img->d_w, img->d_h);

    gettimeofday(&start, NULL);
    uint8_t *yuv420buf = new uint8_t[img->d_w * img->d_h * 3/2];

    const uint8_t *srcLine = (const uint8_t *)img->planes[VPX_PLANE_Y];
    uint8_t *dst = yuv420buf;
    for (size_t i = 0; i < img->d_h; ++i) {
        memcpy(dst, srcLine, img->d_w);
        srcLine += img->stride[VPX_PLANE_Y];
        dst += img->d_w;
    }

    srcLine = (const uint8_t *)img->planes[VPX_PLANE_U];
    for (size_t i = 0; i < img->d_h / 2; ++i) {
        memcpy(dst, srcLine, img->d_w / 2);
        srcLine += img->stride[VPX_PLANE_U];
        dst += img->d_w / 2;
    }

    srcLine = (const uint8_t *)img->planes[VPX_PLANE_V];
    for (size_t i = 0; i < img->d_h / 2; ++i) {
        memcpy(dst, srcLine, img->d_w / 2);
        srcLine += img->stride[VPX_PLANE_V];
        dst += img->d_w / 2;
    }
    //pthread_mutex_unlock (&mCodecLock);
    char path[128];
/*
    sprintf(path, "/data/data/com.xue.talk2.ui/yy%d.yuv", index++);
    FILE *f = fopen(path, "w");
    fwrite(yuv420buf, img->d_w * img->d_h * 3/2, 1, f);
    fclose(f);
*/

    yuv420p_rgb565(yuv420buf, img->d_w, img->d_h, mrgbbuffer);
    delete []yuv420buf;

    //sprintf(path, "/data/data/com.xue.talk2.ui/dd%d.png", index++);
    //saveImage(path, mrgbbuffer, img->d_w, img->d_h);

    d->dataPtr = (uint8_t*)mrgbbuffer;
    d->length = mRGBBufSize;
    d->encLength = length;
    d->w = img->d_w;
    d->h = img->d_h;

    gettimeofday(&end, NULL);
    long time_use = (end.tv_sec-start.tv_sec)*1000000+(end.tv_usec-start.tv_usec);
    LOGD("vp8decode consume:%.2fms, ", (double)time_use/1000.0);
    return 0;
}

void vp8Decoder::yuv420p_rgb565(uint8_t *src, int w, int h, __u16 *dst)
{
    int line, col, linewidth;
    int y, u, v, yy, vr, ug, vg, ub;
    int r, g, b;
    const unsigned char *py, *pu, *pv;

    linewidth = w >> 1;
    py = src;
    pu = py + (w * h);
    pv = pu + (w * h) / 4;

    y = *py++;
    yy = y << 8;
    u = *pu - 128;
    ug =   88 * u;
    ub = 454 * u;
    v = *pv - 128;
    vg = 183 * v;
    vr = 359 * v;

    for (line = 0; line < h; line++) {
        for (col = 0; col < w; col++) {
            b = (yy +      vr) >> 8;
            g = (yy - ug - vg) >> 8;
            r = (yy + ub     ) >> 8;

            if (r < 0)   r = 0;
            if (r > 255) r = 255;
            if (g < 0)   g = 0;
            if (g > 255) g = 255;
            if (b < 0)   b = 0;
            if (b > 255) b = 255;
            *dst++ = (((__u16)r>>3)<<11) | (((__u16)g>>2)<<5) | (((__u16)b>>3)<<0);

            y = *py++;
            yy = y << 8;
            if (col & 1) {
                pu++;
                pv++;

                u = *pu - 128;
                ug =   88 * u;
                ub = 454 * u;
                v = *pv - 128;
                vg = 183 * v;
                vr = 359 * v;
            }
        } 
        if ((line & 1) == 0) { // even line: rewind
            pu -= linewidth;
            pv -= linewidth;
        }
    } 
}

