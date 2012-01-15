package com.xue.talk2.ui;

import java.util.ArrayList;
import java.util.Iterator;

import android.app.Application;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.graphics.Bitmap;
import android.net.ConnectivityManager;
import android.net.NetworkInfo;
import android.os.Handler;
import android.os.Message;
import android.os.PowerManager;
import android.os.PowerManager.WakeLock;
import android.util.Log;

import com.xue.talk2.ui.UserListView.UserInfo;


class Talk2Lib {

	private String TAG="Talk2Lib";
	Handler mUIHandler = null;

	private class Msg{
		int what;
		Object obj;
		int arg1, arg2;
	}
	private boolean mRequestRun;
	
	private Thread mNativeThread;
	private int mPort;
	private String mMyname;
	private boolean mThreadRunning;
	private Context mContext;
	private WakeLock wakeLock;
	private ArrayList<Msg> msgList = new ArrayList<Msg>();

	private IntentFilter mNetworkStateChangedFilter;
	BroadcastReceiver mNetworkStateIntentReceiver = new BroadcastReceiver() {

		public void onReceive(Context context, Intent intent) {
			if (intent.getAction().equals(
					ConnectivityManager.CONNECTIVITY_ACTION)) {

				NetworkInfo info = intent.getParcelableExtra(
						ConnectivityManager.EXTRA_NETWORK_INFO);
				String typeName = info.getTypeName();
				String subtypeName = info.getSubtypeName();

				Log.d("NetworkState", "typeName="+typeName+" subtypeName="+subtypeName);
				
				if(mRequestRun && !mThreadRunning && hasInternat(context)){
					startRun();
				}
				else if(mThreadRunning && !hasInternat(context)){
					stop();
					mNativeThread = null;
				}
			}
		}
	};
	

	public Talk2Lib(Handler handler, Context context){
		mContext = context.getApplicationContext();
		mUIHandler = handler;
		mNetworkStateChangedFilter = new IntentFilter();
		mNetworkStateChangedFilter.addAction(
				ConnectivityManager.CONNECTIVITY_ACTION);
		mContext.registerReceiver(mNetworkStateIntentReceiver,
				mNetworkStateChangedFilter);
	}

	synchronized public void setHandler(Handler handler, Context context){

		if(mContext != null){
			mContext.unregisterReceiver(mNetworkStateIntentReceiver);
		}	

		mContext = context;
		if(mContext != null){
			mContext.registerReceiver(mNetworkStateIntentReceiver,
					mNetworkStateChangedFilter);
		}

		mUIHandler = handler;
		if(mUIHandler != null && msgList.size()>0){
			Iterator<Msg> it = msgList.iterator();
			while (it.hasNext()) {
				Msg msg = (Msg) it.next();
				Message msg1 = mUIHandler.obtainMessage(msg.what);
				msg1.obj = msg.obj;
				msg1.arg1 = msg.arg1;
				msg1.arg2 = msg.arg2;
				msg1.sendToTarget();
			}
			msgList.clear();
		}
	}

	
	public int run(int port, String myname){
		if(mThreadRunning == true) 
			return -1;

		boolean isValid = hasInternat(mContext);
		Log.d(TAG, "isNetworkTypeValid="+isValid);
		
		mPort = port;
		mMyname = myname;
		
		mRequestRun = true;

		if(isValid){
			startRun();
			return 0;
		}

		return 1;
	}

	private void startRun(){
		mThreadRunning = true;
		mNativeThread = new Thread(){
			public void run(){
				Talk2Lib.this.start(mPort, mMyname);
				mThreadRunning = false;
			}
		};
		mNativeThread.start();
	}
	
	public void destroy(){
		releaseWakeLock();
		
		msgList.clear();
		if(mContext != null)
			mContext.unregisterReceiver(mNetworkStateIntentReceiver);

		if(mThreadRunning == true){
			stop();
			try {
				mNativeThread.join(2000);
			} catch (InterruptedException e) {
				e.printStackTrace();
			}
		}
		mNativeThread = null;
		Log.d(TAG, "destroy");

	}
	
	public int hangoffCall(int sessionID){
		delayReleaseWakeLock(10*1000);
		return cancelCall(sessionID);
	}
	
	
	public int startPlayVideo(int sessionID, int start){
		if(mNativeThread == null) return -1;
		
		if(start == 1){
			acquireWakeLock();
		}
		//Log.d(TAG, "===>", new Exception());
		return startPlay(sessionID, start);
	}
	
	public native int call(String targetAddr, int port,
			int acceptMedia, int txMedia,
			int videoW, int videoH);
	
	private native int start(int port, String myname);
	private native int cancelCall(int sessionID);
	private native int startPlay(int sessionID, int start);
	
	private native void stop();

	public native int postVideoPreviewData(byte[] data, int sessionID);

	public native void acceptCall(boolean yes, int sessionID, int videoW, int videoH);

	synchronized private void onInviteeRespArrival(int respCode, 
			int inMedia, int outMedia, int sessionID, String invitee_name){
		
		if(mUIHandler!=null){
			Log.d(TAG, "onInviteeRespArrival code="+respCode);
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_INVITEE_RESP);
			msg.arg1 = respCode;
			msg.arg2 = sessionID;
			msg.obj = invitee_name;
			msg.sendToTarget();
		}
		else{
			Msg m = new Msg();
			m.what = MainUI.MESSAGE_INVITEE_RESP;
			m.obj = null;
			m.arg1 = respCode;
			m.arg2 = sessionID;
			msgList.add(m);
		}
	}

	synchronized private void onConfirmAcceptCall(String name, 
			int acceptMedia, int outMedia, int sessionID, String ipAddr){
		Log.d(TAG, "onConfirmAccepteCall inviter="+name+" sessionID="+sessionID);

		UserInfo user = new UserInfo();
		user.name = name;
		user.ipAddr = ipAddr;
		user.sessionID = sessionID;
		
		if(mUIHandler!=null){
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_CONFIRM_ACCEPT);
			
			msg.obj = user;
			msg.arg1 = sessionID;
			msg.arg2 = acceptMedia;
			msg.sendToTarget();
		}
		else{
			Msg m = new Msg();
			m.what = MainUI.MESSAGE_CONFIRM_ACCEPT;
			m.obj = user;
			m.arg1 = sessionID;
			m.arg2 = acceptMedia;
			msgList.add(m);
		}
	}

	synchronized private void onRGB565BitmapArrival(Bitmap bm, int sessionID, int encodeSize){
		Log.d(TAG, "onRGB565BitmapArrival bmp  w="+bm.getWidth()+" h="+bm.getHeight()+
				" mUIHandler="+mUIHandler);
		
		if(mUIHandler!=null){		
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_SHOW_BITMAP);
			msg.obj = bm;
			msg.arg1 = encodeSize;
			msg.sendToTarget();
		}
		else{
			Msg m = new Msg();
			m.what = MainUI.MESSAGE_SHOW_BITMAP;
			m.obj = bm;
			m.arg1 = encodeSize;
			m.arg2 = 0;
			msgList.add(m);
		}
	}

	synchronized private void notifyEncodedImage(int sessionID, int encodeSize){
		if(mUIHandler!=null){		
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_TX_LENGTH);
			msg.arg1 = encodeSize;
			msg.sendToTarget();
		}
	}
	
	synchronized private void onDetectUser(String username, String ipAddr){
		Log.d(TAG, "onDetectUser user ="+username+" from "+ipAddr);

		UserInfo user = new UserInfo();
		user.name = username;
		user.ipAddr = ipAddr;
		user.description = ipAddr;

		if(mUIHandler!=null){
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_USER_DETECTED);
			msg.obj = user;
			msg.arg1 = 0;
			msg.sendToTarget();
		}
		else{
			Msg m = new Msg();
			m.what = MainUI.MESSAGE_USER_DETECTED;
			m.obj = user;
			m.arg1 = 0;
			m.arg2 = 0;
			msgList.add(m);
		}
	}

	synchronized private void onCallSessionBegin(String ipStr, int sessionID){
		if(mUIHandler!=null){
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_CALL_SESSION_BEGIN);
			msg.obj = ipStr;
			msg.arg1 = sessionID;
			msg.arg2 = 0;
			msg.sendToTarget();
		}
		else{
			Msg m = new Msg();
			m.what = MainUI.MESSAGE_CALL_SESSION_BEGIN;
			m.obj = ipStr;
			m.arg1 = sessionID;
			m.arg2 = 0;
			msgList.add(m);
		}
	}
	
	synchronized private void onSessionDisconnect(int sessionID){
		Log.d(TAG, "onSessionDisconnect user ="+sessionID);

		delayReleaseWakeLock(10*1000);
		
		if(mUIHandler!=null){
			while(mUIHandler.hasMessages(MainUI.MESSAGE_SHOW_BITMAP)){
				mUIHandler.removeMessages(MainUI.MESSAGE_SHOW_BITMAP);
			}

			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_USER_STOP_SESSION);
			msg.obj = null;
			msg.arg1 = sessionID;
			msg.sendToTarget();
		}
		else{
			Msg m = new Msg();
			m.what = MainUI.MESSAGE_USER_STOP_SESSION;
			m.obj = null;
			m.arg1 = sessionID;
			m.arg2 = 0;
			msgList.add(m);
		}
	}

	private void acquireWakeLock() {
		if (wakeLock == null && mContext != null) {
			Log.d(TAG,"Acquiring wake lock");
			PowerManager pm = (PowerManager) mContext.getSystemService(Context.POWER_SERVICE);
			wakeLock = pm.newWakeLock(PowerManager.SCREEN_DIM_WAKE_LOCK, this.getClass().getCanonicalName());
			wakeLock.acquire();
		}
	}

	private void delayReleaseWakeLock(int timeMs){
		if(mUIHandler != null){
			mUIHandler.postDelayed(mDelayReleaseWakelock, timeMs);
		}
		else{
			releaseWakeLock();
		}
	}
	
	private void releaseWakeLock() {
		if (wakeLock != null && wakeLock.isHeld()) {
			Log.d(TAG, "release wake lock");
			wakeLock.release();
			wakeLock = null;
		}

	}
	
	Runnable mDelayReleaseWakelock = new Runnable(){

		public void run() {
			releaseWakeLock();
		}
	};

	synchronized private void onPeriodicTimeout(){
		if(mUIHandler!=null){
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_PERIODIC_TIMER);
			msg.sendToTarget();
		}
	}

	public native void setMyname(String name);
	
	public void setSessionID(int id){
		mSessionID = id;
	}
	
	
	public native int getSessionStatus(int id);
	
	public int getSessionID(){
		return mSessionID;
	}
	
	public boolean hasInternat(Context context){
		if(context == null){
			return false;
		}
		
		ConnectivityManager manager = (ConnectivityManager) context.getSystemService(
				Context.CONNECTIVITY_SERVICE);
		if(manager == null){
			return false;
		}else{
			NetworkInfo[] info = manager.getAllNetworkInfo();
			if(info != null){
				int length = info.length;
				for(int count=0; count<length; count++){
					if(info[count].getState() == NetworkInfo.State.CONNECTED){
						return true;
					}
				}
			}
		}
		return false;
	}
	
	private int mSessionID;
	
	public native void testCodec();

	private int mNativeEncoder;
	private int mNativeSession;

	static{
		System.loadLibrary("talk2jni");
	}
}
