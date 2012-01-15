#include <SkBitmap.h>
extern "C"{
#include "event2/util.h"
#include "event2/bufferevent.h"
#include "event2/event.h"
#include "event2/thread.h"
#include "event2/event_compat.h"

#include <sys/types.h>
#include <sys/socket.h>
//#include <netinet/in.h>
#include <arpa/inet.h>

/* Required by event.h. */
#include <sys/time.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>
#include <jni.h>
}

#include "vp8Encoder.h"
#include "vp8Decoder.h"

#define LOG_TAG "LISTENING_SRV"
#include "log.h"

static unsigned long _myip;
static unsigned long get_ip();
unsigned long get_myip()
{
    if(_myip == 0){
        _myip = get_ip();
    }
    return _myip;
}

/* Port to listen on. */
#define SERVER_PORT 5555
static JNIEnv* jenv;
static JavaVM *gJavaVM = NULL;
static jobject jobj;
static jmethodID   onInviteeRespArrival;
static jmethodID   onDetectUser;
static jmethodID   onConfirmAcceptCall;
static jmethodID   onRGB565BitmapArrival;
static jmethodID   notifyEncodedImage;
static jmethodID   onPeriodicTimeout;
static jmethodID   onSessionDisconnect;
static jmethodID   onCallSessionBegin;
static jclass    bitmapClazz;
static jmethodID bitmapConstructor;
static jmethodID createBitmapMethod;

static uint32_t gsession = 100;

static const char INVITE_CMD = 1;
static const char INVITE_RESP_CMD = 2;
static const char MEDIA_PACKAGE = 3;
static const char ACK_MEDIA_PACKAGE = 4;
static const char RESYNC_CMD = 5;

static const char MAGIC = 0xa5;
static const int32_t CMD_HEADER_SIZE = 4;
static const int32_t CMD_RESYNC_SIZE = 128;

static evutil_socket_t pair[2] = {-1,-1};
enum Media
{
    VIDEO = 0x1,
    AUDIO = 0x2
};

struct CallCmd_t{
    char cmd[8];
    char ipAddr[16];
    int port;
    int acceptMedia, txMedia;
    int videoX, videoY;
};

struct AcceptCallCmd_t{
    char cmd[16];
    int id;
    int yes;
    int acceptMedia, txMedia;
    int videoW, videoH;
};

enum connection{TCP, UDP};
static int setupSocket(struct in_addr bind_addr, int port, enum connection conn);
static int setupSocket(enum connection conn);

#define GET_ENCODER(env, obj) ((vp8Encoder*)(env->GetIntField(obj, fields.nativeEncoder)))
#define GET_SESSION(env, obj) ((TalkSession*)(env->GetIntField(obj, fields.nativeSession)))
#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

struct field {
    const char *class_name;
    const char *field_name;
    const char *field_type;
    jfieldID   *jfield;
};

struct fieldids{
    jfieldID    nativeEncoder;
    //    jfieldID    nativeSession;
} fields;

static int find_fields(JNIEnv *env, field *fields, int count);

struct InviteCmd{
    char inviter_name[32];
    char description[32];
    uint32_t sessionID;
    int in_media_flag, out_media_flag;
    int videoW, videoH;
};

struct InviteRespCmd
{
    enum RespCode{RECV=0, OK=1, REJECT=2} resp_code;
    char invitee_name[32];
    int out_media;
    int accept_media;
    int videoW, videoH;
};


struct AckMediaPackageCmd
{
    enum AckCode{STOP_SEND, NEXTONE, RESTART, FORCE_PAUSE, START_PLAY} ackCode;
    int index;
};

struct TalkBroadcast
{
    char user_name[32];
    char description[32];
    enum {ONLINE, OFFLINE} status;
};

void postCancelSessionMsg(int id)
{
    char buf[128];
    sprintf(buf, "cancelSession:%d", id);
    //LOGD("cmd=%s", buf);
    send(pair[0], buf, strlen(buf), 0);
}

class TalkSession;
class SessionManager
{
    struct SessionList
    {
        struct SessionList *next;
        TalkSession *session;
    };

    public:
    SessionManager(const char* user);
    ~SessionManager();

    void addSession(TalkSession *newSession);

    void removeSession(uint32_t id);

    void removeAll();

    TalkSession *findSession(uint32_t id);
    const char* getMyname();
    void setMyname(const char *name);

    protected:
    void onPeriodicTask();
    void on_receiving_broadcast(int fd, short events);

    static void periodic_timer(int fd, short events, void *arg)
    {
        ((SessionManager*)arg)->onPeriodicTask();
    }

    static void on_broadcast(int fd, short events, void *arg)
    {
        ((SessionManager*)arg)->on_receiving_broadcast(fd, events);
    }
    SessionList *talks;
    pthread_mutex_t mListLock;
    pthread_mutex_t mLock;
    struct event mPeriodicEv;
    struct event *mReadBroadcastEv;
    struct TalkBroadcast mBroadcastInfo;
    int mBroadcastFd;
    struct sockaddr_in mcast_out;
};

class MediaChannel;
class TalkSession
{
    friend class MediaChannel;

    public:
    enum Mode{Server, Client};
    static struct event_base *mbase;
    static SessionManager *mManager;

    public:
    TalkSession(enum Mode mode);
    ~TalkSession();

    uint32_t getSessionID(){return mSessionID;}

    static SessionManager* getSessionManager() {return mManager;}

    void initServer(int client_fd, struct sockaddr_in &addr_in);
    int makeCall(struct CallCmd_t *cc);//const char* ip_addr, int port);
    void acceptCall(bool yes, int , int, int, int);
    void startPlay(int start);
    int cancelCall();
    MediaChannel *getMediaChannel() {return mMediaChannel;}

    void postVideoData(uint8_t *data, int length);
    void postVideoData(JNIEnv * env, jbyteArray data);

    void postDecodeData(JNIEnv *env, uint8_t* data, int length, enum Media media);

    void onDisconnect();

    protected:

    MediaChannel *setupMediaChannel(struct bufferevent *bufferevt);

    void setTimeout(int seconds)
    {
        struct timeval tv;

        LOGD("setTimeout second=%d", seconds);
        tv.tv_usec = 0;
        tv.tv_sec = seconds;
        evtimer_add(&mTimeoutEv, &tv);
    }

    void cancelTimeout()
    {
        LOGD("cancelTimeout()");
        evtimer_del(&mTimeoutEv);
    }

    static void onSessionTimeout(evutil_socket_t fd, short event, void *arg)
    {
        TalkSession *talk = (TalkSession*)arg;
        LOGD("onSessionTimeout()");

        postCancelSessionMsg(talk->getSessionID());
    }

    enum ConnectState
    {
        New, Connectting, SetupConnect,
        CallReqSend, ConnectConfirm, Dead,
        stForcePauseByPeer,
    }mConnectState;


    MediaChannel *mMediaChannel;
    struct event mTimeoutEv;

    enum Mode mMode;
    uint32_t mSessionID;

    struct in_addr mPeerAddr;
};

static const int NOT_RECV_RESYNC_CMD = 0x1;
static const int NOT_RECV_RESYN_CONFIRM_CMD = 0x2;
class DecodedImageQueue;
class EncodedImageQueue;
class MediaChannel
{
    TalkSession *mOwnSession;
    DecodedImageQueue *mDecodedImageQueue;
    EncodedImageQueue *mEncodedImageQueue;

    public:
    MediaChannel(struct bufferevent *bufferevt, TalkSession *talk);
    virtual ~MediaChannel();

    void postEncodedImage(uint8_t *data, int length);
    void postDecodedImage(JNIEnv *env, uint8_t *data, int length);
    void sendInviteCmd(int inMedia, int outMedia);
    void sendInviteRespCmd(InviteRespCmd::RespCode respCode);

    void enterResyncState();
    void sendResyncCmd();
    void sendResyncAckCmd();

    void setupInviteCmd(const char *myname, const char* desc,
            int in_mediaFlag, int out_mediaFlg, struct InviteCmd* ic);

    int setupEncodeDecode();

    void onSessionWrite();

    protected:
    void onStateChange(short what);
    void onSessionRead();
    bool processCmd(int processingCmd, int cmdLength);
    void processInviteCmd(int length);
    void processInviteRespCmd(int length);
    uint32_t setupCmdHeader(int length, const char cmdCode);
    int parseHeader(int32_t raw, uint32_t &length, char &cmdCode);

    public:
    static void sessionState(struct bufferevent *bev, short what, void *ctx)
    {
        ((MediaChannel*)ctx)->onStateChange(what);
    };

    static void sessionRead(struct bufferevent *bev, void *ctx)
    {
        ((MediaChannel*)ctx)->onSessionRead();
    }

    static void sessionWrite(struct bufferevent *bev, void *ctx)
    {
        LOGD("sessionWrite MediaChannel=%p", ctx);
        ((MediaChannel*)ctx)->onSessionWrite();
    }

    void postRawMediaData(JNIEnv * env, jbyteArray data)
    {
        LOGD("postRawMediaData mAllowSend=%d, mRestart=%d, mForcePauseByPeer=%d byMe=%d",
                mAllowSend, mRestart, mForcePauseByPeer, mForcePauseByMe);

        if(mAllowSend && mRestart == 0 && !mForcePauseByPeer && !mForcePauseByMe)
            mEncoder->postData(env, data);

        if(mRestart & NOT_RECV_RESYN_CONFIRM_CMD)
        {
            sendResyncCmd();
        }
    }

    void sendAckMediaPackageCmd(AckMediaPackageCmd::AckCode ackCode, int index, bool check = true);
    void startWork(int start)
    {
        LOGD("MediaChannel::startWork start=%d", start);
        if(start == 1)
        {
            mOwnSession->cancelTimeout();
            mForcePauseByMe = false;
            sendAckMediaPackageCmd(AckMediaPackageCmd::START_PLAY, 0, false);
        }
        else
        {
            mForcePauseByMe = true;
            sendAckMediaPackageCmd(AckMediaPackageCmd::FORCE_PAUSE, 0, false);
            MediaChannel::mOwnSession->setTimeout(30);
        }
    }

    void setupVideoSize(int w, int h){
        mVideoW = w;
        mVideoH = h;
    }

    void enableMedia(int accept, int tx){
        mAcceptMediaBit = accept;
        mTxMediaBit = tx;
    }

    protected:
    void processAckMediaPackage(int length);

    void processMediaPackage(int length);


    protected:
    struct bufferevent *mbufferevt;
    char mProcessingCmd;
    uint32_t mCmdLength;

    private:

    PipeQueueHandler *mEncoder;
    PipeQueueHandler *mDecoder;
    pthread_mutex_t mCodecLock;

    struct event *ev_accept;
    int listen_s;
    uint32_t mMediaPackageIndex, mPostDecodeNum, mSendNum;
    bool mAllowSend;
    bool mForcePauseByPeer, mForcePauseByMe;
    int mRestart;
    uint32_t mRecMediaIndex;
    uint8_t mResyncBuf[CMD_RESYNC_SIZE];
    PipeQueue mSendOutQueue;

    int mAcceptMediaBit;
    int mTxMediaBit;
    int mVideoW, mVideoH;
    int mPeerVideoW, mPeerVideoH;
};

class DecodedImageQueue : public IQueue
{
    public:
        DecodedImageQueue(MediaChannel *mediaChannel)
        {
            mMediaChannel = mediaChannel;
        }

        void appendData(JNIEnv *env, uint8_t *data, int length)
        {
            mMediaChannel->postDecodedImage(env, data, length);
        }

    protected:
        MediaChannel *mMediaChannel;
};

class EncodedImageQueue : public IQueue
{
    public:
        EncodedImageQueue(MediaChannel *mediaChannel)
        {
            mMediaChannel = mediaChannel;
        }

        void appendData(uint8_t *data, int length)
        {
            mMediaChannel->postEncodedImage(data, length);
        }

    protected:
        MediaChannel *mMediaChannel;
};

MediaChannel::MediaChannel(struct bufferevent *bufferevt, TalkSession *session)
    :mSendOutQueue(6, "MediaSendOutQ")
{
    mbufferevt = bufferevt;
    mProcessingCmd = 0;
    mCmdLength = 0;
    mOwnSession = session;
    mEncoder = mDecoder = 0;
    mAllowSend = false;
    mForcePauseByPeer = true;
    mForcePauseByMe = true;

    mDecodedImageQueue = 0;
    mEncodedImageQueue = 0;
    mRestart = 0;
    mRecMediaIndex = 0;
    pthread_mutex_init(&mCodecLock, NULL);
}

MediaChannel::~MediaChannel()
{
    LOGD("Enter ~MediaChannel()");

    delete mEncoder;
    delete mDecoder;
    mEncoder = mDecoder = 0;

    delete mEncodedImageQueue;
    delete mDecodedImageQueue;
    mDecodedImageQueue = 0;
    mEncodedImageQueue = 0;

    if(mbufferevt)
        bufferevent_free(mbufferevt);

    pthread_mutex_destroy(&mCodecLock);
    LOGD("Leave ~MediaChannel()");
}

void MediaChannel::onStateChange(short what)
{
    if (what & BEV_EVENT_CONNECTED) {
        LOGD("====>Connect() return");
        sendInviteCmd(VIDEO, VIDEO);
        return;
    }

    if (what & BEV_EVENT_EOF) {
        /* Client disconnected, remove the read event and the
         * free the client structure. */
        LOGE("Client disconnected.\n");
    }
    else {
        LOGW("Client socket error, disconnecting.\n");
    }

    LOGD("================================================");
    LOGD("  Media (type:CONTROL) channel closed by peer ");
    LOGD("================================================");
    mOwnSession->onDisconnect();

    //mConnectState = Dead;
}

void MediaChannel::onSessionWrite()
{
    LOGD("Enter MediaChannel::onSessionWrite()");
    uint8_t *data;
    int length;

    while(mSendOutQueue.getData(data, length, false) == 0 && mbufferevt)
    {
        evbuffer_add(bufferevent_get_output(mbufferevt), data, length);
    bufferevent_flush(mbufferevt, EV_WRITE, BEV_NORMAL);
        //int ret = bufferevent_write(mbufferevt, data, length);
        LOGD("SEND encoded image length=%d, sendnum=%d", length, mSendNum++);
        mSendOutQueue.readDone();
        jenv->CallVoidMethod(jobj, notifyEncodedImage, mOwnSession->mSessionID, length);
    }

    if(!mForcePauseByPeer && !mForcePauseByMe &&
            mDecoder && mbufferevt &&
            mDecoder->getFreeLength() >= 1)
    {
        sendAckMediaPackageCmd(AckMediaPackageCmd::NEXTONE, 0);
    }

    LOGD("Leave MediaChannel::onSessionWrite()");
}

void MediaChannel::onSessionRead()
{
    while(true)
    {
        int bufsize = evbuffer_get_length(bufferevent_get_input(mbufferevt));
        LOGD("Enter onRead: mProcessingCmd=%d, bufLength=%d",mProcessingCmd, bufsize);

        int32_t header;

        if(mRestart != 0)
        {
            uint8_t *p = (uint8_t*)malloc(bufsize);
            bufferevent_read(mbufferevt, (char*)p, bufsize);
            if(bufsize >= CMD_RESYNC_SIZE)
            {
                memcpy(mResyncBuf, p + bufsize - CMD_RESYNC_SIZE, CMD_RESYNC_SIZE);
            }
            else
            {
                int shift = CMD_RESYNC_SIZE - bufsize;
                memcpy(mResyncBuf, mResyncBuf+bufsize, shift);
                memcpy(mResyncBuf + shift, p, bufsize);
            }

            free(p);
            uint8_t i;
            for(i = 0; i<CMD_RESYNC_SIZE; i++)
            {
                if( mResyncBuf[i] != i )
                    break;
            }

            if(i == CMD_RESYNC_SIZE)
            {
                LOGD("Find Resync cmd");
                mRestart &= ~NOT_RECV_RESYNC_CMD;
                sendResyncAckCmd();
                return;
            }

            for(i = 0; i<CMD_RESYNC_SIZE; i++)
            {
                if( mResyncBuf[i] != CMD_RESYNC_SIZE - 1 - i)
                    break;
            }
            if(i == CMD_RESYNC_SIZE)
            {
                LOGD("Find Resync ack cmd");
                mRestart &= ~NOT_RECV_RESYN_CONFIRM_CMD;
            }
            return;
        }
        else if(mProcessingCmd == 0 && bufsize >= CMD_HEADER_SIZE)
        {
            bufferevent_read(mbufferevt, (char*)&header, sizeof(header));
            if(parseHeader(header, mCmdLength, mProcessingCmd) != 0)
            {
                LOGE("Invalid cmd was found, enter resync state.");
                enterResyncState();
                return;
            }
        }
        else if(mProcessingCmd == 0){
            bufferevent_setwatermark(mbufferevt, EV_READ, CMD_HEADER_SIZE, 0);
            return;
        }

        bufsize = evbuffer_get_length(bufferevent_get_input(mbufferevt));
        LOGD("Receiving cmd(%d), length=%d, bufReadableSize=%d",
                mProcessingCmd, mCmdLength, bufsize);

        if(mCmdLength > bufsize)
        {
            bufferevent_setwatermark(mbufferevt, EV_READ, mCmdLength, 0);
            //MediaChannel::mOwnSession->setTimeout(10);
            return;
        }
        //MediaChannel::mOwnSession->cancelTimeout();
        processCmd(mProcessingCmd, mCmdLength);
        mProcessingCmd = 0;
    }
}

bool MediaChannel::processCmd(int processingCmd, int cmdLength)
{
    bool process = true;
    switch(processingCmd)
    {
        case INVITE_CMD:
            processInviteCmd(mCmdLength);
            break;
        case INVITE_RESP_CMD:
            processInviteRespCmd(cmdLength);
            break;
        case MEDIA_PACKAGE:
            processMediaPackage(cmdLength);
            break;
        case ACK_MEDIA_PACKAGE:
            processAckMediaPackage(cmdLength);
            break;
        default:
            process = false;
    }

    return process;
}

void MediaChannel::processInviteCmd(int length)
{
    if(length != sizeof(struct InviteCmd))
    {
        LOGE("Invalid invite cmd size");
        return;
    }

    struct InviteCmd ic;
    bufferevent_read(mbufferevt, (char*)&ic, sizeof(ic));
    ic.inviter_name[sizeof(ic.inviter_name)-1] = '\0';
    ic.in_media_flag = ntohl(ic.in_media_flag);
    ic.out_media_flag = ntohl(ic.out_media_flag);
    mOwnSession->mSessionID = ntohl(ic.sessionID);
    LOGD("processInviteCmd inviter=%s, sessionid=%d, video:%dx%d",
            ic.inviter_name, mOwnSession->mSessionID,
            ic.videoW, ic.videoH);

    jstring str = jenv->NewStringUTF(ic.inviter_name);
    jstring ipstr = jenv->NewStringUTF(inet_ntoa(mOwnSession->mPeerAddr));

    jenv->CallVoidMethod(jobj, onConfirmAcceptCall, str,
            ic.in_media_flag, ic.out_media_flag,
            mOwnSession->mSessionID, ipstr);

    mPeerVideoW = ntohl(ic.videoW);
    mPeerVideoH = ntohl(ic.videoH);

    sendInviteRespCmd(InviteRespCmd::RECV);
    jenv->DeleteLocalRef(str);
    jenv->DeleteLocalRef(ipstr);
}

void MediaChannel::processInviteRespCmd(int length)
{
    if(length != sizeof(struct InviteRespCmd))
    {
        LOGE("Invalid invite resp cmd size");
        return;
    }

    struct InviteRespCmd irc;
    bufferevent_read(mbufferevt, (char*)&irc, sizeof(irc));
    LOGD("processInviteRespCmd peer respCode=%d, accept_media=%d, out_media=%d",
            irc.resp_code, ntohl(irc.accept_media), ntohl(irc.out_media));

    if(irc.resp_code == InviteRespCmd::OK)
    {
        mPeerVideoW = ntohl(irc.videoW);
        mPeerVideoH = ntohl(irc.videoH);
        setupEncodeDecode();
    }
    else if((irc.resp_code) == InviteRespCmd::REJECT)
    {
        postCancelSessionMsg(mOwnSession->mSessionID);
    }

    jstring str = jenv->NewStringUTF(irc.invitee_name);
    jenv->CallVoidMethod(jobj, onInviteeRespArrival, irc.resp_code,
            ntohl(irc.out_media), ntohl(irc.accept_media),
            mOwnSession->mSessionID, str);
    jenv->DeleteLocalRef(str);
}

void MediaChannel::processMediaPackage(int length)
{
    uint32_t mediaIndex;
    int n = bufferevent_read(mbufferevt, (char*)&mediaIndex, sizeof(mediaIndex));

    if(mRestart != 0) return;

    mediaIndex = ntohl(mediaIndex);
    int diff =  mRecMediaIndex - mediaIndex;
    if(diff > 3 || diff < -3)
    {
        LOGD("media index mis-sync mRecMediaIndex=%d, mediaIndex=%d", mRecMediaIndex, mediaIndex);
        enterResyncState();
        return;
    }

    mRecMediaIndex = mediaIndex;
    if(mDecoder)
    {
        int freeSlot = mDecoder->postData(mbufferevt, length-4);
        LOGD("RECV MediaPackage len=%d, mediaIndex=%d, freeSlot=%d",
            length-4, mediaIndex, freeSlot);
        if(freeSlot >= 3 && !mForcePauseByPeer && !mForcePauseByMe)
        {
            sendAckMediaPackageCmd(AckMediaPackageCmd::NEXTONE, mediaIndex);
            return;
        }
    }

    if(!mForcePauseByPeer && !mForcePauseByMe)
        sendAckMediaPackageCmd(AckMediaPackageCmd::STOP_SEND, 0);
}

void MediaChannel::processAckMediaPackage(int length)
{
    struct AckMediaPackageCmd amc;
    bufferevent_read(mbufferevt, (char*)&amc, sizeof(amc));
    amc.ackCode = (AckMediaPackageCmd::AckCode)ntohl(amc.ackCode);
    LOGD("PEER ackCode=%d, ackNo=%d", amc.ackCode, ntohl(amc.index));

    if(amc.ackCode == AckMediaPackageCmd::STOP_SEND)
    {
        mAllowSend = false;
    }
    else if(amc.ackCode == AckMediaPackageCmd::FORCE_PAUSE)
    {
        mForcePauseByPeer = true;
        //mOwnSession->setTimeout(10);
    }
    else if(amc.ackCode == AckMediaPackageCmd::START_PLAY)
    {
        mOwnSession->cancelTimeout();
        mForcePauseByPeer = false;
        mAllowSend = true;
    }
    else if (amc.ackCode == AckMediaPackageCmd::RESTART)
    {
        enterResyncState();
    }
    else if (amc.ackCode == AckMediaPackageCmd::NEXTONE)
    {
        mRestart = false;
        mAllowSend = true;
    }
}

void MediaChannel::enterResyncState()
{
    mRestart = NOT_RECV_RESYNC_CMD | NOT_RECV_RESYN_CONFIRM_CMD;
    sendAckMediaPackageCmd(AckMediaPackageCmd::RESTART, 0);
    mDecoder->cleanAllPostData();
    mEncoder->cleanAllPostData();
    bufferevent_setwatermark(mbufferevt, EV_READ, 1, 0);
    mProcessingCmd = 0;
}

void MediaChannel::sendResyncCmd()
{
    uint8_t resync_cmd[CMD_RESYNC_SIZE];

    for(uint8_t i = 0; i<CMD_RESYNC_SIZE; i++)
    {
        resync_cmd[i] = i;
    }

    bufferevent_write(mbufferevt, resync_cmd, CMD_RESYNC_SIZE);

    int bufsize = evbuffer_get_length(bufferevent_get_output(mbufferevt));
    bufferevent_flush(mbufferevt, EV_WRITE, BEV_NORMAL);
}

void MediaChannel::sendResyncAckCmd()
{
    uint8_t resync_cmd[CMD_RESYNC_SIZE];

    for(uint8_t i = 0; i<CMD_RESYNC_SIZE; i++)
    {
        resync_cmd[i] = CMD_RESYNC_SIZE - 1 - i;
    }

    bufferevent_write(mbufferevt, resync_cmd, CMD_RESYNC_SIZE);
    int bufsize = evbuffer_get_length(bufferevent_get_output(mbufferevt));
    bufferevent_flush(mbufferevt, EV_WRITE, BEV_NORMAL);
}

void MediaChannel::sendInviteCmd(int inMedia, int outMedia)
{
    LOGD("MediaChannel::sendInviteCmd()");
    int bufsize = sizeof(struct InviteCmd) + CMD_HEADER_SIZE;
    uint8_t *buf = new uint8_t[bufsize];

    uint32_t header = setupCmdHeader(sizeof(struct InviteCmd), INVITE_CMD);
    *((int*)buf) = header;

    struct InviteCmd* ic = (struct InviteCmd*)&buf[CMD_HEADER_SIZE];

    setupInviteCmd(TalkSession::mManager->getMyname(), "test", inMedia, outMedia, ic);

    if(mbufferevt){
        evbuffer_add(bufferevent_get_output(mbufferevt), buf, bufsize);
    }

    delete []buf;
}

void MediaChannel::sendInviteRespCmd(InviteRespCmd::RespCode respCode)
{
    LOGD("MediaChannel::sendInviteRespCmd()");
    int bufsize = sizeof(struct InviteRespCmd) + CMD_HEADER_SIZE;
    uint8_t *buf = new uint8_t[bufsize];

    uint32_t header = setupCmdHeader(sizeof(struct InviteRespCmd),
            INVITE_RESP_CMD);
    *((int*)buf) = header;

    struct InviteRespCmd* irc = (struct InviteRespCmd*)&buf[CMD_HEADER_SIZE];
    irc->resp_code = (respCode);
    memset(irc->invitee_name, sizeof(irc->invitee_name), '\0');
    memcpy(irc->invitee_name,
            TalkSession::mManager->getMyname(),
            sizeof(irc->invitee_name)-1);
    irc->accept_media = htonl(mAcceptMediaBit);
    irc->out_media = htonl(mTxMediaBit);
    irc->videoW = htonl(mVideoW);
    irc->videoH = htonl(mVideoH);

    if(mbufferevt){
        evbuffer_add(bufferevent_get_output(mbufferevt), buf, bufsize);
        //bufferevent_write(mbufferevt, buf, bufsize);
    }

    delete []buf;
}

void MediaChannel::setupInviteCmd(const char *myname, const char* desc,
        int in_mediaFlag, int out_mediaFlag, struct InviteCmd* ic)
{
    strncpy(ic->inviter_name, myname, sizeof(ic->inviter_name)-1);
    ic->inviter_name[sizeof(ic->inviter_name)-1] = '\0';
    strncpy(ic->description, desc, sizeof(ic->description)-1);
    ic->description[sizeof(ic->description)-1] = '\0';

    ic->sessionID = htonl(mOwnSession->mSessionID);
    ic->in_media_flag = htonl(mAcceptMediaBit);
    ic->out_media_flag = htonl(mTxMediaBit);
    ic->videoW = htonl(mVideoW);
    ic->videoH = htonl(mVideoH);
}

int gAckNo=0;
void MediaChannel::sendAckMediaPackageCmd(AckMediaPackageCmd::AckCode ackCode, int index, bool check)
{
    int bufsize = sizeof(struct AckMediaPackageCmd) + CMD_HEADER_SIZE;
    uint8_t *buf = new uint8_t[bufsize];

    uint32_t header = setupCmdHeader(sizeof(struct AckMediaPackageCmd),
            ACK_MEDIA_PACKAGE);
    *((int*)buf) = header;

    struct AckMediaPackageCmd *pc = (struct AckMediaPackageCmd*)&buf[CMD_HEADER_SIZE];
    pc->ackCode = (AckMediaPackageCmd::AckCode)htonl(ackCode);
    pc->index = htonl(gAckNo);

    if(mbufferevt){
        evbuffer_add(bufferevent_get_output(mbufferevt), buf, bufsize);
        //bufsize = evbuffer_get_length(bufferevent_get_output(mbufferevt));
        LOGD("sendAckMediaPackage ackNo=%d, ackCode=%d.",
            gAckNo, ackCode);
        //bufferevent_write(mbufferevt, buf, bufsize);
        //bufferevent_flush(mbufferevt, EV_WRITE, BEV_FLUSH);
    }
    gAckNo++;

    delete []buf;
}

void MediaChannel::postEncodedImage(uint8_t *data, int length)
{
    uint32_t header = setupCmdHeader(length+4, MEDIA_PACKAGE);
    mMediaPackageIndex++;
    uint32_t mediaSeq = htonl(mMediaPackageIndex);
    LOGD("POST encode image to peer, length=%d, postNum=%d",
            length, mMediaPackageIndex);

    uint8_t *p = (uint8_t*)malloc(length+8);
    *((uint32_t*)p) = header;
    *((uint32_t*)(p+4)) = mediaSeq;
    memcpy(p+8, data, length);
    mSendOutQueue.postData(p, length+8);
    free(p);

    char buf[64];
    sprintf(buf, "write:%d", mOwnSession->getSessionID());
    send(pair[0], buf, strlen(buf), 0);
}

void MediaChannel::postDecodedImage(JNIEnv *env, uint8_t *data, int length)
{
    LOGD("SEND Bitmap to java: length=%d, postnum=%d", length, mPostDecodeNum++);
    mOwnSession->postDecodeData(env, data, length, VIDEO);
}

int MediaChannel::setupEncodeDecode()
{
    if(mTxMediaBit | VIDEO)
    {
        LOGD("Setup encoder %dx%d", mVideoW, mVideoH);
        mEncodedImageQueue = new EncodedImageQueue(this);
        mEncoder = new vp8Encoder(mVideoW, mVideoH, mEncodedImageQueue, mCodecLock);
        mEncoder->start();
    }

    if(mAcceptMediaBit | VIDEO)
    {
        LOGD("Setup decoder %dx%d", mPeerVideoW, mPeerVideoH);
        mDecodedImageQueue = new DecodedImageQueue(this);
        mDecoder = new vp8DecoderThread(mPeerVideoW, mPeerVideoH, mDecodedImageQueue, gJavaVM, 6, mCodecLock);
        mDecoder->start();
    }
    mSendNum = mMediaPackageIndex = mPostDecodeNum = 0;
}

uint32_t MediaChannel::setupCmdHeader(int length, const char cmdCode)
{
    uint32_t raw = ( length & 0xfffff )<<12 | (cmdCode & 0xf)<<8 | MAGIC;
    return htonl(raw);
}

int MediaChannel::parseHeader(int32_t raw, uint32_t &length, char &cmdCode)
{
    raw = ntohl(raw);
    LOGD("Parse Header DWORD=0x%x, magic=0x%x", raw, raw & 0xff);
    if((raw & 0xff) == MAGIC)
    {
        cmdCode = (char)(raw >> 8 & 0xf);
        length = (int)(raw >> 12 & 0xfffff);
        return 0;
    }
    LOGE("It is not header DWORD");
    return -1;
}

    TalkSession::TalkSession(enum Mode mode)
:mMode(mode)
{

    if(mode == Client){
        mConnectState = New;
        mSessionID = gsession++;
    }
    else{
        mSessionID = 0;
        mConnectState = SetupConnect;
    }
    evtimer_assign(&mTimeoutEv, TalkSession::mbase, onSessionTimeout, this);
    mMediaChannel = NULL;
};

TalkSession::~TalkSession()
{
    LOGD("Enter ~TalkSession()");
    evtimer_del(&mTimeoutEv);
    delete mMediaChannel;
    mMediaChannel = 0;
    //delete mVideoChannel;
    LOGD("Leave ~TalkSession()");
}

void TalkSession::initServer(int client_fd, struct sockaddr_in &addr_in)
{
    mPeerAddr = addr_in.sin_addr;

    if (evutil_make_socket_nonblocking (client_fd) < 0)
        LOGW("failed to set client socket non-blocking");

    evutil_make_listen_socket_reuseable(client_fd);

    struct bufferevent *connectbufEv = bufferevent_socket_new(mbase,
            client_fd,
            BEV_OPT_CLOSE_ON_FREE);// | BEV_OPT_THREADSAFE);

    mMediaChannel = setupMediaChannel(connectbufEv);
};

int TalkSession::makeCall(struct CallCmd_t *cc)//const char* ip_addr, int port)
{
    if(mMode == Server)
        return -1;

    LOGD("makeCall");

    struct bufferevent *connectbufEv = bufferevent_socket_new(mbase,
            -1, BEV_OPT_CLOSE_ON_FREE);// | BEV_OPT_THREADSAFE);
    if(connectbufEv == NULL)
    {
        LOGE("bufferevent_socket_new() return err");
        return -1;
    }
    mConnectState = New;
    mSessionID = gsession++;

    struct sockaddr_in svr_addr;

    svr_addr.sin_family = AF_INET;
    svr_addr.sin_addr.s_addr = inet_addr(cc->ipAddr);
    svr_addr.sin_port = htons(cc->port);

    mMediaChannel = setupMediaChannel(connectbufEv);
    mMediaChannel->setupVideoSize(cc->videoX, cc->videoY);
    mMediaChannel->enableMedia(cc->acceptMedia, cc->txMedia);

    if(bufferevent_socket_connect(connectbufEv,
                (struct sockaddr *)&svr_addr,
                sizeof(svr_addr)) == 0)
    {
        mPeerAddr = svr_addr.sin_addr;
        mConnectState = Connectting;
        return 0;
    }

    LOGD("make call return -1");
    return -1;
};

int TalkSession::cancelCall()
{
    LOGD("cancelCall");
    return 0;
}

void TalkSession::acceptCall(bool yes, int acceptMedia, int txMedia, int videoX, int videoY)
{
    LOGD("accept call video: %dx%d, yes=%d", videoX, videoY, yes);
    if(yes == true)
    {
        mMediaChannel->setupVideoSize(videoX, videoY);
        mMediaChannel->enableMedia(acceptMedia, txMedia);
        mMediaChannel->sendInviteRespCmd(InviteRespCmd::OK);
        mMediaChannel->setupEncodeDecode();
    }
    else
    {
        mMediaChannel->sendInviteRespCmd(InviteRespCmd::REJECT);
        postCancelSessionMsg(mSessionID);
    }
}

void TalkSession::startPlay(int start)
{
    LOGD("TalkSession::startPlay start=%d", start);
    mMediaChannel->startWork(start);
}

/*void TalkSession::postVideoData(uint8_t *data, int length)
{
    mMediaChannel->postRawMediaData(data, length);
}*/

void TalkSession::postVideoData(JNIEnv * env, jbyteArray data)
{
    mMediaChannel->postRawMediaData(env, data);
}

void TalkSession::postDecodeData(JNIEnv *env, uint8_t* data, int length, enum Media media)
{
    LOGD("TalkSession::postDecodeData() length=%d, env=%p", length, env);
    if(data == 0 || length == 0 || env == 0)
        return;

    if(media == VIDEO)
    {
        struct DecodeData *d = (struct DecodeData*)data;

        SkBitmap *bitmap = new SkBitmap();
        if (bitmap == NULL) {
            LOGE("postDecodeData: cannot instantiate a SkBitmap object.");
            return;
        }

        bitmap->setConfig(SkBitmap::kRGB_565_Config, d->w, d->h);
        if (!bitmap->allocPixels()) {
            delete bitmap;
            LOGE("failed to allocate pixel buffer");
            return ;
        }
        memcpy((uint8_t*)bitmap->getPixels(), (uint8_t*)d->dataPtr, d->length);

        // Since internally SkBitmap uses reference count to manage the reference to
        // its pixels, it is important that the pixels (along with SkBitmap) be
        // available after creating the Bitmap is returned to Java app.
        jobject jSrcBitmap = env->NewObject(bitmapClazz,
                bitmapConstructor, (int) bitmap, true, NULL, -1);
        env->CallVoidMethod(jobj, onRGB565BitmapArrival, jSrcBitmap, mSessionID, d->encLength);
        env->DeleteLocalRef(jSrcBitmap);
        /*jbyteArray bArray = env->NewByteArray(d->length);

          env->SetByteArrayRegion(bArray, 0, d->length, (jbyte *)(d->dataPtr));
          env->CallVoidMethod(jobj, onRGB565BitmapArrival, bArray, d->w, d->h, mSessionID);
          env->DeleteLocalRef(bArray);*/
    }
}

void TalkSession::onDisconnect()
{
    jenv->CallVoidMethod(jobj, onSessionDisconnect, mSessionID);
    postCancelSessionMsg(mSessionID);

    /*char buf[128];
    sprintf(buf, "cancelSession:%d", mSessionID);
    LOGD("cmd=%s", buf);
    send(pair[0], buf, strlen(buf), 0);
    */
}

MediaChannel *TalkSession::setupMediaChannel(struct bufferevent *bufferevt)
{
    MediaChannel *lcb = new MediaChannel(bufferevt, this);
    bufferevent_setcb(bufferevt, MediaChannel::sessionRead,
            0,//MediaChannel::sessionWrite,
            MediaChannel::sessionState,
            lcb);
    bufferevent_setwatermark(bufferevt, EV_READ, CMD_HEADER_SIZE, 0);

    bufferevent_enable(bufferevt, EV_READ|EV_WRITE);
    return lcb;

}



#define HELLO_PORT 5345
#define HELLO_GROUP "224.0.0.251"

SessionManager::SessionManager(const char *user)
{
    talks = 0;
    mBroadcastFd = 0;
    mReadBroadcastEv = 0;
    memset(&mBroadcastInfo, sizeof(mBroadcastInfo), '\0');
    memcpy(mBroadcastInfo.user_name, user, sizeof(mBroadcastInfo.user_name)-1);

    pthread_mutex_init(&mListLock, NULL);
    pthread_mutex_init(&mLock, NULL);

    int send_fd = setupSocket(UDP);
    u_char loop = 0;
    setsockopt(send_fd, IPPROTO_IP, IP_MULTICAST_LOOP, &loop, sizeof(loop));

    int recv_fd = setupSocket(UDP);

    unsigned char ttl = 32;
    int optres2 = setsockopt(recv_fd,IPPROTO_IP,IP_MULTICAST_TTL,&ttl,sizeof(ttl));
    if (optres2 == -1)
    {
        int perrno = errno;
        LOGD("Error setsockopt: ERRNO = %s\n",strerror(perrno));
        EVUTIL_CLOSESOCKET(recv_fd);
        EVUTIL_CLOSESOCKET(send_fd);
        return;
    }

    struct ip_mreq mreq;
    memset(&mreq, '\0', sizeof(mreq));
    inet_aton(HELLO_GROUP, &mreq.imr_multiaddr);
    mreq.imr_interface.s_addr = get_myip();

    if (setsockopt(recv_fd,IPPROTO_IP,IP_ADD_MEMBERSHIP,&mreq,sizeof(mreq)) < 0) {
        LOGE("Multicast setsockopt errno=%d:%s", errno, strerror(errno));
        EVUTIL_CLOSESOCKET(recv_fd);
        EVUTIL_CLOSESOCKET(send_fd);
        return;
    }

    mcast_out.sin_family = AF_INET;
    mcast_out.sin_port = htons(HELLO_PORT);
    mcast_out.sin_addr.s_addr = mreq.imr_multiaddr.s_addr;

    if ( setsockopt(recv_fd, IPPROTO_IP, IP_MULTICAST_IF,
                &mreq.imr_interface.s_addr, sizeof(mreq.imr_interface.s_addr)) != 0 )
    {
        LOGE("setsockopt IP_MULTICAST_IF, errno=%d:%s", errno, strerror(errno));
        EVUTIL_CLOSESOCKET(recv_fd);
        EVUTIL_CLOSESOCKET(send_fd);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(HELLO_PORT);
    addr.sin_addr.s_addr = mreq.imr_multiaddr.s_addr;

    if (bind(recv_fd,(struct sockaddr *) &addr, sizeof(addr)) < 0) {
        perror("bind");
        LOGE("bind, errno=%d:%s", errno, strerror(errno));
        EVUTIL_CLOSESOCKET(recv_fd);
        EVUTIL_CLOSESOCKET(send_fd);
        return;
    }

    evtimer_assign(&mPeriodicEv, TalkSession::mbase, periodic_timer, this);
    struct timeval tv;

    tv.tv_usec = 0;
    tv.tv_sec = 2;
    evtimer_add(&mPeriodicEv, &tv);

    mBroadcastFd = send_fd;
    mReadBroadcastEv = event_new(TalkSession::mbase, recv_fd, EV_READ|EV_PERSIST, on_broadcast, this);
    event_add(mReadBroadcastEv, NULL);
}

void SessionManager::on_receiving_broadcast(int fd, short events)
{
    struct sockaddr_in addr;
    int addrlen=sizeof(addr);
    int nbytes;

    struct TalkBroadcast msg;

    if ((nbytes=recvfrom(fd,(void*)&msg,sizeof(struct TalkBroadcast), 0,
                    (struct sockaddr *) &addr,&addrlen)) < 0) {
    }

    msg.user_name[sizeof(msg.user_name) - 1] = '\0';
    LOGD("Receiving broadcast %s from %s", msg.user_name, inet_ntoa(addr.sin_addr));

    jstring user = jenv->NewStringUTF(msg.user_name);
    jstring ipAddr = jenv->NewStringUTF(inet_ntoa(addr.sin_addr));
    jenv->CallVoidMethod(jobj, onDetectUser, user, ipAddr);
    jenv->DeleteLocalRef(user);
    jenv->DeleteLocalRef(ipAddr);
}

const char* SessionManager::getMyname()
{
    return &mBroadcastInfo.user_name[0];
}

void SessionManager::setMyname(const char *name)
{
    pthread_mutex_lock (&mLock);
    memset(&mBroadcastInfo.user_name, sizeof(mBroadcastInfo.user_name), '\0');
    memcpy(mBroadcastInfo.user_name, name, sizeof(mBroadcastInfo.user_name)-1);
    pthread_mutex_unlock (&mLock);
}

SessionManager::~SessionManager()
{
    LOGD("enter ~SessionManager(), mBroadcastFd=%d", mBroadcastFd);
    if(mBroadcastFd)
    {
        EVUTIL_CLOSESOCKET(mBroadcastFd);
        event_del(&mPeriodicEv);
        event_del(mReadBroadcastEv);
        event_free(mReadBroadcastEv);
        mBroadcastFd = 0;
    }

    removeAll();
    pthread_mutex_destroy(&mListLock);
    pthread_mutex_destroy(&mLock);

    TalkSession::mManager = 0;
    LOGD("leave ~SessionManager()");
};

void SessionManager::onPeriodicTask()
{
    pthread_mutex_lock (&mLock);
    if (sendto(mBroadcastFd, (void*)&mBroadcastInfo, sizeof(mBroadcastInfo),0,
                (struct sockaddr *) &mcast_out,
                sizeof(mcast_out)) < 0) {
        LOGE("broadcast sendto error, %s", strerror(errno));
    }
    pthread_mutex_unlock (&mLock);

    struct timeval tv;
    tv.tv_usec = 0;
    tv.tv_sec = 5;
    evtimer_add(&mPeriodicEv, &tv);

    jenv->CallVoidMethod(jobj, onPeriodicTimeout);
}

void SessionManager::addSession(TalkSession *newSession)
{
    pthread_mutex_lock (&mListLock);
    if(findSession(newSession->getSessionID()) != 0)
    {
        LOGW("The session already in list");
        pthread_mutex_unlock (&mListLock);
        return;
    }

    struct SessionList *l = talks;

    while(l != 0 && l->next != 0)
    {
        l = l->next;
    }

    struct SessionList *n = new struct SessionList;
    n->session = newSession;
    n->next = 0;

    if(l == 0)
        talks = n;
    else
        l->next = n;

    pthread_mutex_unlock (&mListLock);
}

void SessionManager::removeSession(uint32_t id)
{
    pthread_mutex_lock (&mListLock);
    struct SessionList *l = talks;
    struct SessionList *p = 0;
    for(; l != 0; p=l, l=l->next)
    {
        if(l->session->getSessionID() == id)
        {
            if(p != 0)
            {
                p->next = l->next;
            }
            else
            {
                talks = 0;
            }
            LOGD("Delete session %d form manager", id);
            delete l->session;
            delete l;
            pthread_mutex_unlock (&mListLock);
            return;
        }
    }

    LOGW("Can't remove session id=%d", id);
    pthread_mutex_unlock (&mListLock);
}

void SessionManager::removeAll()
{
    LOGD("enter removeAll()");
    pthread_mutex_lock (&mListLock);
    struct SessionList *l = talks;
    for(; l != 0; l=l->next)
    {
        struct SessionList *n = l->next;
        LOGD("Delete session %d form manager", l->session->getSessionID());
        delete l->session;
        delete l;
        if(!n) break;
        l = n;
    }
    talks = 0;
    pthread_mutex_unlock (&mListLock);
    LOGD("leave removeAll()");
}

TalkSession *SessionManager::findSession(uint32_t id)
{
    for(struct SessionList *l = talks; l != 0; l=l->next)
    {
        if(l->session->getSessionID() == id)
            return l->session;
    }
    return 0;
}


/**
 * This function will be called by libevent when there is a connection
 * ready to be accepted.
 */
void on_accept(int fd, short ev, void *arg)
{
    int client_fd;
    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);

    client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
        LOGW("accept failed");
        return;
    }

    SessionManager *sessionMgr = (SessionManager*)arg;
    TalkSession *session = new TalkSession(TalkSession::Server);
    session->initServer(client_fd, client_addr);
    sessionMgr->addSession(session);
    //jenv->SetIntField(jobj, fields.nativeSession, (jint)session);

    LOGD("Accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));
}

static void control_channel_read(evutil_socket_t fd, short event, void *arg)
{
    char buf[128];
    ssize_t readed = read(fd, buf, 128);

    if(readed <= 0 && readed == 128)
        return;

    buf[readed] = '\0';
    LOGD("control_channel_read():[%s]", buf);

    if(memcmp(buf, "call:", strlen("call:")) == 0)
    {
        TalkSession *talkSession = new TalkSession(TalkSession::Client);
        struct CallCmd_t *cc = (struct CallCmd_t *)&buf;

        LOGD("talkSession=0x%x, addr_str=%s, port=%d, videoW=%d, videoY=%d", 
                talkSession, cc->ipAddr, cc->port,
                cc->videoX, cc->videoY);

        jstring ipstr = jenv->NewStringUTF(cc->ipAddr);

        if(talkSession->makeCall(cc) == 0)
        {
            TalkSession::mManager->addSession(talkSession);
            jenv->CallVoidMethod(jobj, onCallSessionBegin, ipstr, talkSession->getSessionID());
        }
        else
        {
            delete talkSession;
            jenv->CallVoidMethod(jobj, onCallSessionBegin, ipstr,-1);
        }
        jenv->DeleteLocalRef(ipstr);
    }
    else if(memcmp(buf, "cancelSession:", strlen("cancelSession:")) == 0)
    {
        uint32_t sessionid;
        sscanf(buf, "cancelSession:%d", &sessionid);
        LOGD("Cancel session=%d", sessionid);

        TalkSession::mManager->removeSession(sessionid);
        //TalkSession *talk = GET_SESSION(jenv, jobj);
        //if(talk) delete talk;
        //jenv->SetIntField(jobj, fields.nativeSession, (jint)0);
    }
    else if(memcmp(buf, "write:", strlen("write:")) == 0)
    {
        MediaChannel *ch;
        int id;
        sscanf(buf, "write:%d", &id);
        TalkSession *t = TalkSession::mManager->findSession(id);
        if(t){
            LOGD("call onSessionWrite()");
            MediaChannel *m = t->getMediaChannel();
            if(m) m->onSessionWrite();
        }
    }
    else if(memcmp(buf, "acceptCall:", strlen("acceptCall:")) == 0)
    {
        struct AcceptCallCmd_t *acc = (struct AcceptCallCmd_t *)&buf;
        LOGD("acceptCallCmd yes=%d, videoW=%d, videoH=%d", acc->yes,
                acc->id, acc->videoW, acc->videoH);
        int sessionID = acc->id;
        TalkSession *talk = TalkSession::mManager->findSession(sessionID);
        if(!talk)
        {
            LOGE("Can't find session id=%d in acceptCall", sessionID);
            return;
        }

        talk->acceptCall(acc->yes==1, acc->acceptMedia, acc->txMedia, acc->videoW, acc->videoH);
    }
    else if(memcmp(buf, "startPlay:", strlen("startPlay:")) == 0)
    {
        TalkSession *talk;
        int start;

        sscanf(buf, "startPlay:%d:%d", &talk, &start);
        LOGD("startPlay:start=%d", start);

        talk->startPlay(start);
    }
    else if(memcmp(buf, "quit", 4) == 0)
    {
        event_base_loopexit(TalkSession::mbase, 0);
    }
}

static void write_to_android_log_cb(int severity, const char *msg)
{
    switch (severity) {
        case _EVENT_LOG_DEBUG:
        case _EVENT_LOG_MSG:
            LOGD("libevent:%s", msg);
            break;

        case _EVENT_LOG_WARN:
            LOGW("libevent:%s", msg);
            break;
        case _EVENT_LOG_ERR:
        default:
            LOGE("libevent:%s", msg);
            break;
    }
}

SessionManager *TalkSession::mManager = NULL;
struct event_base *TalkSession::mbase = NULL;


extern "C" jint Java_com_xue_talk2_ui_Talk2Lib_start(
        JNIEnv * env, jobject obj, jint port, jstring jmyname)
{
    int listen_fd;
    struct sockaddr_in listen_addr;
    struct event *ev_accept;

    field fields_to_find[] = {
        {"com/xue/talk2/ui/Talk2Lib", "mNativeEncoder",   "I", &fields.nativeEncoder },
        //{"com/xue/talk2/ui/Talk2Lib", "mNativeSession",   "I", &fields.nativeSession },
    };


    if (find_fields(env, fields_to_find, COUNT_OF(fields_to_find)) < 0)
        return -1;

    jenv = env;
    jobj = obj;

    jclass cls = env->GetObjectClass(obj);
    onInviteeRespArrival = env->GetMethodID(cls,
            "onInviteeRespArrival", "(IIIILjava/lang/String;)V");
    if(onInviteeRespArrival == 0)
    {
        LOGE("Can't find method 'onInviteeRespArrival'");
        return -1;
    }

    onDetectUser = env->GetMethodID(cls,
            "onDetectUser", "(Ljava/lang/String;Ljava/lang/String;)V");
    if(onDetectUser == 0)
    {
        LOGE("Can't find method 'onDetectUser'");
        return -1;
    }

    onConfirmAcceptCall = env->GetMethodID(cls,
            "onConfirmAcceptCall", "(Ljava/lang/String;IIILjava/lang/String;)V");
    if(onConfirmAcceptCall == 0)
    {
        LOGE("Can't find method 'onConfirmAcceptCall'");
        return -1;
    }

    onPeriodicTimeout = env->GetMethodID(cls,
            "onPeriodicTimeout", "()V");
    if(onPeriodicTimeout == 0)
    {
        LOGE("Can't find method 'onPeriodicTimeout'");
        return -1;
    }

    onCallSessionBegin = env->GetMethodID(cls, "onCallSessionBegin",
            "(Ljava/lang/String;I)V");
    if(onCallSessionBegin == 0)
    {
        LOGE("Can't find method 'onCallSessionBegin'");
        return -1;
    }


    onSessionDisconnect = env->GetMethodID(cls,
            "onSessionDisconnect", "(I)V");
    if(onSessionDisconnect == 0)
    {
        LOGE("Can't find method 'onSessionDisconnect'");
        return -1;
    }

    onRGB565BitmapArrival = env->GetMethodID(cls,
            "onRGB565BitmapArrival", "(Landroid/graphics/Bitmap;II)V");
    if(onRGB565BitmapArrival == 0)
    {
        LOGE("Can't find method 'onRGB565BitmapArrival'");
        return -1;
    }

    notifyEncodedImage = env->GetMethodID(cls,
            "notifyEncodedImage", "(II)V");
    if(notifyEncodedImage == 0)
    {
        LOGE("Can't find method 'notifyEncodedImage'");
        return -1;
    }

    bitmapClazz = env->FindClass("android/graphics/Bitmap");
    if (bitmapClazz == NULL) {
        LOGE("Can't find android/graphics/Bitmap");
        return -1;
    }

    bitmapConstructor = env->GetMethodID(bitmapClazz, "<init>", "(IZ[BI)V");
    if (bitmapConstructor == NULL) {
        LOGE("Can't find Bitmap constructor");
        return -1;
    }

    if(get_myip() == 0)
    {
        LOGD("Can't find working network");
        return -2;
    }

    /*evthread_enable_lock_debuging();
    if (evthread_use_pthreads() != 0)
    {
        LOGE("Init evthread_use_pthreads() failed");
        return -3;
    }*/

    //event_set_log_callback(write_to_android_log_cb);
    //event_enable_debug_mode();

    struct event_base *base = event_base_new();
    TalkSession::mbase = base;

    struct in_addr sin_addr;
    sin_addr.s_addr = htonl(INADDR_ANY);
    listen_fd = setupSocket(sin_addr, port, TCP);

    if(listen_fd < 0)
    {
        return -2;
    }

    int len = env->GetStringUTFLength(jmyname);
    const char* myname = env->GetStringUTFChars(jmyname, NULL);
    SessionManager *sessionMgr = new SessionManager(myname);
    env->ReleaseStringUTFChars(jmyname, myname);

    TalkSession::mManager = sessionMgr;

    /* We now have a listening socket, we create a read event to
     * be notified when a client connects. */
    ev_accept = event_new(base, listen_fd, EV_READ|EV_PERSIST, on_accept, sessionMgr);
    event_add(ev_accept, NULL);

    if (evutil_socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == -1) {
        LOGE("socketpair");
    }
    struct event *ev = event_new(base, pair[1], EV_READ|EV_ET|EV_PERSIST,
            control_channel_read, &ev);
    event_add(ev, NULL);

    LOGD("Listening server start to run, listening on %d", port);
    event_base_dispatch(base);


    if(sessionMgr != NULL){
        delete sessionMgr;
        sessionMgr = NULL;
    }

    event_del(ev);
    event_free(ev);
    event_del(ev_accept);
    event_free(ev_accept);

    event_base_free(base);
    base = NULL;
    close(pair[0]);
    close(pair[1]);
    EVUTIL_CLOSESOCKET(listen_fd);

    LOGD("Listening server stop");
    return 0;
}

extern "C" void Java_com_xue_talk2_ui_Talk2Lib_stop(
        JNIEnv * env, jobject obj)
{
    if(TalkSession::mbase != NULL){
        send(pair[0], "quit", 4, 0);
    }
    vp8Encoder *vp8 = GET_ENCODER(env, obj);
    if(vp8) delete vp8;
    env->SetIntField(obj, fields.nativeEncoder, 0);
}

extern "C" jint Java_com_xue_talk2_ui_Talk2Lib_call(
        JNIEnv * env, jobject obj, jstring addr, jint port,
        jint ac, jint tx, jint vx, jint vy)
{
    if(TalkSession::mManager == 0)
    {
        LOGE("Initilize function failed. Please call start() firstly");
        return -1;
    }

    LOGD("Java_com_xue_talk2_ui_Talk2Lib_call");

    //int len = env->GetStringUTFLength(addr);
    const char* addr_str = env->GetStringUTFChars(addr, NULL);
    char buf[128];
    struct CallCmd_t call;
    memset(&call, 0, sizeof(call));
    strcpy(call.cmd, "call:");
    strcpy(call.ipAddr, addr_str);
    call.port = port;
    call.acceptMedia = ac;
    call.txMedia = tx;
    call.videoX = vx;
    call.videoY = vy;
    //sprintf(buf, "call:%d:%s:%d", talkSession, addr_str, port);
    //LOGD("Call cmd buf=%s", buf);
    env->ReleaseStringUTFChars(addr, addr_str);

    //send(pair[0], buf, strlen(buf), 0);
    send(pair[0], &call, sizeof(call), 0);

    return 0;//talkSession->getSessionID();
}


extern "C" jint Java_com_xue_talk2_ui_Talk2Lib_cancelCall(
        JNIEnv * env, jobject obj, jint sessionid)
{
    if(TalkSession::mManager == 0)
    {
        LOGE("Initilize function failed. Please call start() firstly");
        return -1;
    }
    LOGD("cancelCall sessionID=%d", sessionid);
    TalkSession *talkSession = TalkSession::mManager->findSession(sessionid);

    if(!talkSession)
    {
        LOGE("Can't find session id=%d in cancelCall", sessionid);
        return -1;
    }

    postCancelSessionMsg(sessionid);
    return 0;
}

extern "C" void Java_com_xue_talk2_ui_Talk2Lib_acceptCall(
        JNIEnv * env, jobject obj, jboolean yes, jint sessionID,
        jint vx, jint vy)
{
    if(TalkSession::mManager == 0)
    {
        LOGE("Initilize function failed. Please call start() firstly");
        return;
    }

    LOGD("acceptCall yes=%d, sessionID=%d", yes, sessionID);
    TalkSession *talkSession = TalkSession::mManager->findSession(sessionID);
    //TalkSession *talkSession = GET_SESSION(env, obj);

    struct AcceptCallCmd_t acc;
    strcpy(acc.cmd, "acceptCall:");
    acc.id = sessionID;
    acc.yes = yes;
    acc.acceptMedia = VIDEO;
    acc.txMedia = VIDEO;
    acc.videoW = vx;
    acc.videoH = vy;
    send(pair[0], &acc, sizeof(acc), 0);
}

extern "C" void Java_com_xue_talk2_ui_Talk2Lib_startPlay(
        JNIEnv * env, jobject obj, jint sessionID, int start)
{
    LOGD("Java_com_xue_talk2_ui_Talk2Lib_startPlay sessionID=%d, start=%d",
            sessionID, start);

    if(TalkSession::mManager == 0)
    {
        LOGE("Initilize function failed. Please call start() firstly");
        return;
    }

    TalkSession *talkSession = TalkSession::mManager->findSession(sessionID);
    if(!talkSession)
    {
        LOGE("Can't find session id=%d in startPlay", sessionID);
        return;
    }
    char buf[128];
    sprintf(buf, "startPlay:%d:%d", talkSession, start);

    LOGD("cmd=%s", buf);
    send(pair[0], buf, strlen(buf), 0);
}

extern "C" int Java_com_xue_talk2_ui_Talk2Lib_postVideoPreviewData(
        JNIEnv * env, jobject obj, jbyteArray data, jint sessionID)
{
    if(TalkSession::mManager == 0)
    {
        LOGE("Initilize function failed. Please call start() firstly");
        return -1;
    }

    LOGD("postVideoPreviewData sessionID=%d", sessionID);
    TalkSession *talkSession = TalkSession::mManager->findSession(sessionID);

    if(!talkSession)
    {
        LOGE("Can't find session id=%d in postVideoPreviewData", sessionID);
        return -1;
    }

    talkSession->postVideoData(env, data);
    return 0;
}

extern "C" jint Java_com_xue_talk2_ui_Talk2Lib_getSessionStatus(
        JNIEnv *env, jobject obj, jint sessionID)
{
    if(TalkSession::mManager == 0)
    {
        LOGE("Initilize function failed. Please call start() firstly");
        return 0;
    }

    TalkSession *talkSession = TalkSession::mManager->findSession(sessionID);
    LOGD("getSessionStatus sessionID=%d, status=%d", 
            sessionID, talkSession?1:0);
    return talkSession?1:0;
}

extern "C" void Java_com_xue_talk2_ui_Talk2Lib_setMyname(
        JNIEnv * env, jobject obj, jstring jmyname)
{
    if(TalkSession::mManager && jmyname != NULL)
    {
        const char* myname = env->GetStringUTFChars(jmyname, NULL);
        TalkSession::mManager->setMyname(myname);
        env->ReleaseStringUTFChars(jmyname, myname);
    }

    return;
}

extern "C" void Java_com_xue_talk2_ui_Talk2Lib_testCodec(
        JNIEnv *env, jobject obj)
{
    //vp8Encoder *encoder = new vp8Encoder(320, 240, 0, (void*)1);
    //encoder->start();
    //env->SetIntField(obj, fields.nativeEncoder, (jint)encoder);
    //mDecoder = new vp8Decoder(320, 240, 0, gJavaVM, (void*)2);
    //mDecoder->start();
}

extern "C" jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
    LOGD("JNI_OnLoad is called");
    gJavaVM = vm;
    return JNI_VERSION_1_6;
}

extern "C" void JNI_OnUnload(JavaVM* vm, void* reserved)
{
    LOGD("JNI_OnUnload is called");
    gJavaVM = 0;
}

int setupSocket(enum connection conn)
{
    int listen_fd;

    if(conn == TCP)
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    else
        listen_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (listen_fd < 0){
        LOGE("create socket failed");
        return -1;
    }

    if (evutil_make_socket_nonblocking (listen_fd) < 0)
    {
        EVUTIL_CLOSESOCKET(listen_fd);
        LOGE("failed to set server socket to non-blocking");
        return -1;
    }

    if (evutil_make_listen_socket_reuseable (listen_fd) < 0)
    {
        EVUTIL_CLOSESOCKET(listen_fd);
        LOGE("failed to set server socket to reusable");
        return -1;
    }

    return (listen_fd);
}

int setupSocket(struct in_addr bind_addr, int port, enum connection conn)
{
    int listen_fd;

    if(conn == TCP)
        listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    else
        listen_fd = socket(AF_INET, SOCK_DGRAM, 0);

    if (listen_fd < 0){
        LOGE("create socket failed");
        return -1;
    }

    if (evutil_make_socket_nonblocking (listen_fd) < 0)
        LOGE("failed to set server socket to non-blocking");

    struct sockaddr_in listen_addr;
    evutil_make_listen_socket_reuseable(listen_fd);

    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    //listen_addr.sin_addr = bind_addr;
    listen_addr.sin_port = htons(port);
    if (bind(listen_fd, (struct sockaddr *)&listen_addr,
                sizeof(listen_addr)) < 0)
    {
        EVUTIL_CLOSESOCKET(listen_fd);
        LOGE("bind failed:%s", strerror(errno));
        return -1;
    }

    if(conn == TCP){
        if (listen(listen_fd, 2) < 0){
            EVUTIL_CLOSESOCKET(listen_fd);
            LOGE("listen failed");
            return -1;
        }
    }
    return listen_fd;
}

#define MAXINTERFACES 16
#include <linux/if.h>
static unsigned long get_ip()
{
    int sock_fd;
    struct ifreq buf[MAXINTERFACES];
    struct ifconf ifc;
    int interface_num;
    char *addr;//[ADDR_LEN];

    if((sock_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        LOGD("Create socket failed");
        return 0;
    }
    ifc.ifc_len = sizeof(buf);
    ifc.ifc_req = buf;
    if(ioctl(sock_fd, SIOCGIFCONF, (char *)&ifc) < 0)
    {
        LOGD("Get a list of interface addresses failed");
        return 0;
    }

    interface_num = ifc.ifc_len / sizeof(struct ifreq);
    LOGD("The number of interfaces is %d\n", interface_num);

    unsigned long loop_addr = inet_addr("127.0.0.1");
    unsigned long myip = 0;
    while(interface_num--)
    {
        LOGD("Net device: %s\n", buf[interface_num].ifr_name);

        if(ioctl(sock_fd, SIOCGIFFLAGS, (char *)&buf[interface_num]) < 0)
        {
            LOGD("Get the active flag word of the device");
            continue;
        }
        if(buf[interface_num].ifr_flags & IFF_PROMISC)
            LOGD("Interface is in promiscuous mode\n");

        int running;
        if(buf[interface_num].ifr_flags & IFF_UP)
        {
            running = 1;
            LOGD("Interface is running\n");
        }
        else
        {
            running = 0;
            LOGD("Interface is not running\n");
        }

        if(ioctl(sock_fd, SIOCGIFADDR, (char *)&buf[interface_num]) < 0)
        {
            LOGD("Get interface address failed");
            continue;
        }

        struct in_addr ip_addr = ((struct sockaddr_in*)(&buf[interface_num].ifr_addr))->sin_addr;
        addr = inet_ntoa(ip_addr);
        LOGD("IP address is %s, 0x%x, loop_addr=0x%x", addr, ip_addr.s_addr, loop_addr);

        if(running == 1 && (ip_addr.s_addr != loop_addr))
        {
            myip = ip_addr.s_addr;
            addr = inet_ntoa(ip_addr);
            LOGD("MY IP address is %s\n", addr);
            break;
        }
    }

    close(sock_fd);
    return myip;
}

static int find_fields(JNIEnv *env, field *fields, int count)
{
    for (int i = 0; i < count; i++) {
        field *f = &fields[i];
        jclass clazz = env->FindClass(f->class_name);
        if (clazz == NULL) {
            LOGE("Can't find %s", f->class_name);
            return -1;
        }

        jfieldID field = env->GetFieldID(clazz, f->field_name, f->field_type);
        if (field == NULL) {
            LOGE("Can't find %s.%s", f->class_name, f->field_name);
            return -1;
        }

        *(f->jfield) = field;
    }

    return 0;
}
