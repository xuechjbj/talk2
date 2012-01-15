#ifndef _IENCODER_HEADER_
#define _IENCODER_HEADER_

extern "C"{
#include "event2/util.h"
#include "event2/bufferevent.h"
#include "event2/buffer.h"
#include "event2/bufferevent_struct.h"
#include "event2/event.h"
#include <pthread.h>
#include <jni.h>
}
#include "unistd.h"

#define LOG_TAG "PipeQueue"
#include "log.h"
const int DEFAULT_QUEUE_LENGTH = 5;
const int MAX_QUEUE_LENGTH = 20;

class PipeQueue
{
    public:
        /*PipeQueue()
        {
            LOGD("Enter PipeQueue::PipeQueue()");
            mQueueLength = DEFAULT_QUEUE_LENGTH;
            init(0);
            LOGD("Leave PipeQueue::PipeQueue()");
        }*/
        
        PipeQueue(int qLength, const char *handlerName)
        {
            LOGD("Enter PipeQueue::PipeQueue()");
            mQueueLength = DEFAULT_QUEUE_LENGTH;
            if(qLength > 0 && qLength <= MAX_QUEUE_LENGTH)
                mQueueLength = qLength;
            init(handlerName);
            LOGD("Leave PipeQueue::PipeQueue()");
        }

        void init(const char *handlerName)
        {
            if(handlerName == 0)
                mQueueName = "";
            else
                mQueueName = handlerName;

            mFreeSlot = mQueueLength;
            for(int i=0; i< mQueueLength; i++)
            {
                entries[i] = 0;
                data_length[i] = 0;
                buf_length[i] = 0;
            }
            read_index = 0;
            write_index = 0;

            pthread_mutex_init(&lock, NULL);
            pthread_cond_init(&data_coming_condition, NULL);
            bFull = false;
        }

        virtual ~PipeQueue()
        {
            LOGD("%s enter ~PipeQueue() this=%p", mQueueName, this);
            for(int i=0; i< mQueueLength; i++)
            {
                free(entries[i]);
                entries[i] = 0;
                data_length[i] = 0;
                buf_length[i] = 0;
            }
            pthread_mutex_destroy(&lock);
            pthread_cond_destroy(&data_coming_condition);
            LOGD("%s leave ~PipeQueue()", mQueueName);
        }

        int getQueueLength() { return mQueueLength; }
        int getFreeLength() { return mFreeSlot;}

        void cleanAllPostData()
        {
            LOGD("%s enter cleanAllPostData() this=%p", mQueueName, this);
            pthread_mutex_lock (&lock);
            for(int i=0; i< mQueueLength; i++)
            {
                free(entries[i]);
                entries[i] = 0;
                data_length[i] = 0;
                buf_length[i] = 0;
            }
            mFreeSlot = mQueueLength;
            read_index = 0;
            write_index = 0;
            bFull = false;
            pthread_mutex_unlock (&lock);
            LOGD("%s leave ~cleanAllPostData()", mQueueName);
        }

        int postData(struct bufferevent* bufevent, int length)
        {
            return doPostData(NULL, bufevent, NULL, length, 0);
        }

        int postData(uint8_t *data, int length)
        {
            return doPostData(NULL, NULL, data, length, 0);
        }

        int postData(JNIEnv * env, jbyteArray data)//(uint8_t *data, int length)
        {
            jint len = env->GetArrayLength(data);

            return doPostData(data, NULL, NULL, len, env);
        }

        bool isFull()
        {
            return bFull;
        }

    protected:
        int doPostData(jbyteArray jdata, struct bufferevent* bufevent, uint8_t *data, int length, JNIEnv * env)
        {
            if(length <= 0) return -1;

            pthread_mutex_lock (&lock);
            if(write_index == read_index &&  bFull == true)
            {
                if(bufevent != NULL)
                {
                    uint8_t *p = (uint8_t*)malloc(length);
                    bufferevent_read(bufevent, p, length);
                    free(p);
                }

                LOGW("Queue is full. discard input");
                pthread_mutex_unlock (&lock);
                return -1;
            }

            uint8_t* p = entries[write_index];
            if(buf_length[write_index] < length)
            {
                if(p != NULL) free(p);
                p = (uint8_t*)malloc(length);
                buf_length[write_index] = length;
                entries[write_index] = p;
            }

            if(jdata != NULL)
            {
                env->GetByteArrayRegion(jdata, 0, length, (jbyte*)p);
            }
            else if(bufevent != NULL)
            {
                int n = bufferevent_read(bufevent, p, length);
                LOGD("Read length=%d data from bufferevent, expected=%d", n, length);
            }
            else if ( data != NULL && length > 0)
            {
                memcpy(p, data, length);
            }

            data_length[write_index] = length;

            write_index++;
            if(write_index >= mQueueLength)
                write_index = 0;

            if(write_index == read_index)
                bFull = true;

            mFreeSlot--;

            pthread_cond_signal( &data_coming_condition );

            pthread_mutex_unlock (&lock);
            return mFreeSlot;
        };

    public:
        int getData( uint8_t *&data, int &length, bool wait)
        {
            //LOGD("%s enter getData wait=%d", mQueueName, wait);
            pthread_mutex_lock (&lock);
            if(write_index == read_index && !bFull)
            {
                if(wait == false)
                {
                    LOGD("Leave getData() = -1");
                    pthread_mutex_unlock (&lock);
                    return -1;
                }

                LOGD("Begin to waiting... write_index=%d, read_index=%d, bFull=%d", 
                        write_index, read_index, bFull);
                pthread_cond_wait( &data_coming_condition, &lock );
                LOGD("Leave to waiting... write_index=%d, read_index=%d, bFull=%d", 
                        write_index, read_index, bFull);
            }
            data = entries[read_index];
            length = data_length[read_index];

            //LOGD("%s leave getData() at %d data, length=%d", mQueueName, read_index, length);
            return 0;
        }

        bool readDone()
        {
            //LOGD("%s enter readDone", mQueueName);
            if(write_index == read_index && !bFull)
            {
                pthread_mutex_unlock (&lock);
                //LOGD("Leave readDone");
                return false;
            }
            read_index++;
            if(read_index >= mQueueLength)
                read_index = 0;
            bFull = false;
            mFreeSlot++;
            pthread_mutex_unlock (&lock);
            //LOGD("%s leave readDone", mQueueName);
            return read_index != write_index;
        }

    protected:
        uint8_t *entries[MAX_QUEUE_LENGTH];
        int buf_length[MAX_QUEUE_LENGTH];
        int data_length[MAX_QUEUE_LENGTH];
        int read_index;
        int write_index;
        pthread_mutex_t lock;
        pthread_cond_t data_coming_condition;
        bool bFull;
        int mQueueLength, mFreeSlot;
        const char *mQueueName;
};

class PipeQueueHandler : public PipeQueue
{
    public:
        PipeQueueHandler(JavaVM *jvm, const char *handlerName, int qLen=0)
            :PipeQueue(qLen, handlerName)
        {
            mThread = 0;
            mJvm = jvm;
            mEnv = 0;
        }

        virtual ~PipeQueueHandler()
        {
            LOGD("enter ~PipeQueueHandler() this=%p", this);
            mAlive = false;
            if(mThread != 0 )
            {
                pthread_cond_signal( &data_coming_condition );
                pthread_join( mThread, NULL);
                mThread = NULL;
            }
            destory();
            LOGD("leave ~PipeQueueHandler()");
        }

        int start()
        {
            mAlive = true;
            pthread_create( &mThread, NULL, entry_func, this);
        }

        virtual int initilize() = 0;
        virtual void destory() {};
        virtual int process_data(uint8_t *data, int length) = 0;

    protected:
        JavaVM *mJvm;
        JNIEnv *mEnv;
        pthread_t mThread;
        bool mAlive;

        void startWork()
        {
            LOGD("enter PipeQueueHandler::startWork() name=%s", mQueueName);
            if(mAlive == false || initilize() < 0)
            {
                LOGE("leave PipeQueueHandler(%s): initilize failed", mQueueName);
                return;
            }

            if(mJvm)
            {
                int res = mJvm->AttachCurrentThread(&mEnv, NULL);
                if(res <0)
                {
                    LOGE("leave PipeQueueHandler(%s): Cannot attach thread", mQueueName);
                    return;
                }
            }

            while(mAlive)
            {
                uint8_t *data;
                int length;
                if(getData(data, length, true)<0 || mAlive == false)
                    break;

                process_data(data, length);
                readDone();
                usleep(5000);
            }

            if(mJvm)
                mJvm->DetachCurrentThread();

            LOGD("leave PipeQueueHandler name=%s", mQueueName);
        }

        static void* entry_func(void *arg)
        {
            ((PipeQueueHandler*)arg)->startWork();
        }

};

class IQueue
{
    public:
        //virtual int writeDataTo( struct bufferevent* bufevent );
        virtual void appendData(JNIEnv *env, uint8_t *data, int length){};
        virtual void appendData(uint8_t *data, int length){};
        //virtual bool isFull();
};

#undef LOG_TAG
#endif

