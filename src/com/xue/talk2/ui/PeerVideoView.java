package com.xue.talk2.ui;

import android.content.Context;
import android.graphics.PixelFormat;
import android.os.Handler;
import android.os.Message;
import android.util.AttributeSet;
import android.util.Log;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public class PeerVideoView extends SurfaceView implements SurfaceHolder.Callback{

	private static final String TAG="PeerVideoView";
	private boolean mSurfaceReady;
	private Handler mUIHandler;
	private int mSessionID;
	
	public PeerVideoView(Context context, AttributeSet attr){
		super(context, attr);
		
		Log.d(TAG, "PeerVideoView(context)"+context);
		
		getHolder().addCallback(this);
		getHolder().setType(SurfaceHolder.SURFACE_TYPE_NORMAL);
		//getHolder().setFormat(PixelFormat.RGBA_8888);
		mSurfaceReady = false;
		mSessionID = 0;
	}

	protected void onAttachedToWindow (){
		super.onAttachedToWindow();
		Log.d(TAG, "onAttachedToWindow()");
	}
	
	public boolean isReadyDraw(){
		return mSurfaceReady;
	}
	
	public void surfaceChanged(SurfaceHolder arg0, int arg1, int arg2, int arg3) {
		
	}

	
	synchronized public void surfaceCreated(SurfaceHolder arg0) {
		Log.d(TAG, "surfaceCreated()");
		mSurfaceReady = true;
		if(mSessionID != 0 && mUIHandler != null){
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_START_PLAY);
			msg.arg1 = mSessionID;
			msg.arg2 = 1;
			msg.sendToTarget();
		}
	}

	public void surfaceDestroyed(SurfaceHolder arg0) {
		Log.d(TAG, "surfaceDestroyed()");
		mSurfaceReady = false;
		if(mSessionID != 0 && mUIHandler != null){
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_START_PLAY);
			msg.arg1 = mSessionID;
			msg.arg2 = 0;
			msg.sendToTarget();
		}
	}

	synchronized public void setHandler(Handler msgHandler, int sessionID) {
		mUIHandler = msgHandler;
		mSessionID = sessionID;
		if(mSurfaceReady == true){
			Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_START_PLAY);
			msg.arg1 = mSessionID;
			msg.arg2 = 1;
			msg.sendToTarget();
		}
	}
	
	protected void onFinishInflate (){
		super.onFinishInflate();
		Log.d(TAG, "onFinishInflate");
	}
}
