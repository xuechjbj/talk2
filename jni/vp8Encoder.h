#ifndef _VP8_ENCODER_HEADER_
#define _VP8_ENCODER_HEADER_

#include "PipeQueue.h"
extern "C"
{
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"
}

class vp8Encoder : public PipeQueueHandler
{
    public:
        vp8Encoder(int frameWidth, int frameHeight, IQueue *outChannel, pthread_mutex_t &lock);
        ~vp8Encoder();


        int initilize()
        {
            return initilize(display_width, display_height);
        }

        int process_data(uint8_t *data, int length)
        {
            if(!mOutput) return -1;

            return encode(0, data);
        }

        int initilize(int frameWidth, int frameHeight);
        int encode(int buffer_time, uint8_t* video_data);
        void destory();

    private:

        vpx_codec_ctx_t        encoder;
        vpx_image_t raw;
        //PACKETIZER x;

        int request_recovery;
        int gold_recovery_seq;
        int altref_recovery_seq;

        int display_width;
        int display_height;
        int capture_frame_rate;
        int video_bitrate;
        int fec_numerator;
        int fec_denominator;
        int frame_cnt;
        IQueue *mOutput;
        pthread_mutex_t &mCodecLock;
};

#endif
