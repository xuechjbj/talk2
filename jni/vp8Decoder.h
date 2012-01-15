#ifndef _VP8_DECODER_HEADER_
#define _VP8_DECODER_HEADER_

extern "C"
{
#include "vpx/vpx_decoder.h"
#include "vpx/vp8dx.h"
}
#include "PipeQueue.h"

#include <SkImageEncoder.h>
#include <SkBitmap.h>

struct DecodeData
{
    uint8_t *dataPtr;
    uint32_t length;
    uint32_t encLength;
    uint32_t w, h;
};

class vp8Decoder
{
    public:
        vp8Decoder(int frameWidth, int frameHeight, pthread_mutex_t &lock);//, IQueue *outChannel, JavaVM *jvm);
        ~vp8Decoder();

        int initilize();

        int decode(uint8_t *data, int length, struct DecodeData *d);

        void destory();
        void saveImage(const char *path, uint8_t* image, int w, int h);

    protected:
        vpx_codec_ctx_t       decoder;

        vp8_postproc_cfg_t	  ppcfg;
        vpx_codec_dec_cfg_t   cfg ;

        int display_width;
        int display_height;
        int index;
        __u16* mrgbbuffer;
        int mRGBBufSize;
        pthread_mutex_t &mCodecLock;

        void yuv420p_rgb565(uint8_t *src, int w, int h, __u16 *dst);
};

class vp8DecoderThread : public PipeQueueHandler
{
    public:
        vp8DecoderThread(int frameWidth, int frameHeight, IQueue *outChannel, JavaVM *jvm, int qLen, pthread_mutex_t &lock)
            :PipeQueueHandler(jvm, "vp8Decoder", qLen)
        {
            mOutput = outChannel;
            mDecoder = new vp8Decoder(frameWidth, frameHeight, lock);
            index = 0;
        }

        ~vp8DecoderThread()
        {
        }

        int initilize()
        {
            if(mDecoder)
                return mDecoder->initilize();

            return -1;
        }

        int process_data(uint8_t *data, int length)
        {
            if(mDecoder)
            {
                struct DecodeData d;
                int ret = mDecoder->decode(data, length, &d);
                if(ret == 0)
                {
                    //char path[128];
                    //sprintf(path, "/data/data/com.xue.talk2.ui/dd%d.png", index++);
                    //saveImage(path, d.dataPtr, d.w, d.h);
                    mOutput->appendData(mEnv, (uint8_t*)&d, d.length);
                }
                return ret;
            }
            return -1;
        }

        void destory()
        {
            mDecoder->destory();
            delete mDecoder;
        }

        void saveImage(const char *path, uint8_t* image, int w, int h)
        {
            SkBitmap bitmap;
            bitmap.setConfig(SkBitmap::kRGB_565_Config, w, h);
            bitmap.setPixels(image);
            SkImageEncoder::EncodeFile(path, bitmap,
                    SkImageEncoder::kPNG_Type, SkImageEncoder::kDefaultQuality);

        }

    private:
        vp8Decoder *mDecoder;
        IQueue *mOutput;
        int index;
};

#undef LOG_TAG
#endif

