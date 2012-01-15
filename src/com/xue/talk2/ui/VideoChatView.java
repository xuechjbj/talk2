package com.xue.talk2.ui;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

import android.app.AlertDialog;
import android.content.Context;
import android.content.DialogInterface;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Paint;
import android.graphics.PixelFormat;
import android.graphics.Rect;
import android.hardware.Camera;
import android.hardware.Camera.Parameters;
import android.hardware.Camera.PreviewCallback;
import android.hardware.Camera.Size;
import android.os.Environment;
import android.os.Handler;
import android.os.Message;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.TextView;


public class VideoChatView extends FrameLayout implements 
SurfaceHolder.Callback, PreviewCallback, FlingGallery.FlingableView{

	private final String TAG = "VideoChatView";
	private static final int IDEAL_FRAME_WIDTH = 640;
	private static final int IDEAL_FRAME_HEIGHT = 480;

	SurfaceView mCameraPreview;
	SurfaceHolder mCameraPreviewHolder;
	PreviewSize mPreviewSize;
	List<Size> mSupportedPreviewSizes;
	Camera mCamera;
	Handler mMsgHandler;

	PeerVideoView mRemotePeerView;

	private int mSessionID;

	private ViewGroup mainView;
	//private SurfaceView mCameraPreview;
	private boolean mConfigureChangeRestart;

	private TextView mRxInfo1, mRxInfo2, mRxInfo3, mPeerNameView;
	private TextView mTxInfo1, mTxInfo2, mTxInfo3;
	private Button mStopBtn, mPhotoBtn;
	private Talk2Lib mLib;
	private boolean mRemoteViewReady;
	private Context mContext;

	private boolean mTakePhoto;
	private boolean mSurfaceDestroy = true;
	
	class SavedState{
		int sessionid;
		Camera camera;
		String peerName;
	}


	class PreviewSize{
		public PreviewSize(int w, int h){
			width = w; height = h;
		}
		int width;
		int height;
	}

	public VideoChatView(Context context, Handler msgHandler) {
		super(context);

		Log.d(TAG, "VideoChatView constructor");
		mContext = context;
		mConfigureChangeRestart = false;
		mLib = null;
		mMsgHandler = msgHandler;

		LayoutInflater layoutInflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

		mainView = (ViewGroup) layoutInflater.inflate(R.layout.video_chat_view, null);

		//mFrameRate = (TextView)mainView.findViewById(R.id.rx_frame_rate);
		mRxInfo1 = (TextView)mainView.findViewById(R.id.rx_info1);
		mRxInfo2 = (TextView)mainView.findViewById(R.id.rx_info2);
		mRxInfo3 = (TextView)mainView.findViewById(R.id.rx_info3);
		
		mPeerNameView = (TextView)mainView.findViewById(R.id.peer_name);
		//mTxFrameRate = (TextView)mainView.findViewById(R.id.tx_frame_rate);
		mTxInfo1 = (TextView)mainView.findViewById(R.id.tx_info1);
		mTxInfo2 = (TextView)mainView.findViewById(R.id.tx_info2);
		mTxInfo3 = (TextView)mainView.findViewById(R.id.tx_info3);

		mCameraPreview = (SurfaceView)mainView.findViewById(R.id.preview);
		mCameraPreviewHolder = mCameraPreview.getHolder();
		mCameraPreviewHolder.addCallback(this);
		mCameraPreviewHolder.setType(SurfaceHolder.SURFACE_TYPE_PUSH_BUFFERS);

		mRemotePeerView = (PeerVideoView)mainView.findViewById(R.id.preview3);

		LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
				LinearLayout.LayoutParams.MATCH_PARENT,
				LinearLayout.LayoutParams.MATCH_PARENT);

		addView(mainView, layoutParams);

		mStopBtn = (Button)mainView.findViewById(R.id.stop_btn);
		mStopBtn.setOnClickListener(new OnClickListener(){

			public void onClick(View arg0) {
				AlertDialog.Builder builder = new AlertDialog.Builder(getContext()); 
				builder.setMessage("Are you sure you want to stop chat?") 
				.setCancelable(false) 
				.setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener() { 
					public void onClick(DialogInterface dialog, int id) { 
						Message msg = mMsgHandler.obtainMessage(MainUI.MESSAGE_USER_STOP_SESSION);
						msg.arg1 = mSessionID;
						msg.sendToTarget();
					} 
				}) 
				.setNegativeButton(android.R.string.no, new DialogInterface.OnClickListener() { 
					public void onClick(DialogInterface dialog, int id) { 
						dialog.cancel(); 
					} 
				}); 
				AlertDialog alert = builder.create();
				alert.show();
			}
		});

		/*mPhotoBtn = ((Button)mainView.findViewById(R.id.photo_btn));
		mPhotoBtn.setOnClickListener(new OnClickListener(){
			public void onClick(View arg0) {
				Log.d(TAG, "talk photho");
				mTakePhoto = true;
			}
		});*/
		mTakePhoto = false;
		//this.setBackgroundResource(R.drawable.default_bg);
	}

	public void setTalk2Lib(Talk2Lib l){
		mLib = l;
		mSessionID = mLib.getSessionID();
		mRemotePeerView.setHandler(mMsgHandler, mSessionID);
	}

	public void setSavedState(Object saved){
		SavedState lastSaved = (SavedState)saved;
		if(lastSaved!=null){
			mSessionID = lastSaved.sessionid;
			mCamera = lastSaved.camera;
			mPeerName = lastSaved.peerName;
			mPeerNameView.setText(mPeerName);;
			//mRemotePeerView.setHandler(mMsgHandler, mSessionID);
		}
	}	

	public void onVisible(boolean yes){
		Log.d(TAG, "onVisible="+yes);
		if(yes == true){
			if(mCamera == null){
				openCamera(mCameraPreviewHolder);
			}
			
			if(mSurfaceDestroy == false)
			{
				Log.d(TAG, "restart preview");
				startPreview(mCameraPreviewHolder);		
			}
		}
		else{
			if(mCamera != null){
				Log.d(TAG, "stop preview");
				mCamera.setPreviewCallback(null);
				mCamera.stopPreview();
			}
			mSessionID = 0;
		}
	}

	public void startPlay(int sessionid, String peerName){
		mSessionID = sessionid;
		Log.d(TAG, "start play sessionid="+sessionid);
		mRemotePeerView.setHandler(mMsgHandler, sessionid);
		mPeerName = peerName;
		mPeerNameView.setText(mPeerName);
	}

	public int getSessionID(){
		return mSessionID;
	}

	public Object onRetainNonConfigurationInstance (){
		SavedState saved = new SavedState();
		saved.sessionid = mSessionID;
		saved.camera = mCamera;
		saved.peerName = mPeerName;
		mConfigureChangeRestart = true;
		return saved;	
	}

	int index=0;
	String mPeerName;
	
	public void showBitmapStream(Bitmap bm, int encodeSize){
		if(mRemotePeerView == null || !mRemotePeerView.isReadyDraw() || mSessionID == 0) 
			return;

		updateRxFrameUI(encodeSize);

		/*if(Environment.getExternalStorageState() == Environment.MEDIA_MOUNTED){
			mPhotoBtn.setEnabled(true);
		}
		else{
			mPhotoBtn.setEnabled(false);
		}
		
		if(mTakePhoto == true){
			mTakePhoto = false;
			java.io.FilenameFilter filter = new java.io.FilenameFilter() {
                public boolean accept(File dir, String name) {
                    return name.endsWith("talk2"+mPeerName+"*.png");
                }
            };
            File path = Environment.getExternalStoragePublicDirectory(
                    Environment.DIRECTORY_PICTURES);
            File dir = new File(path, "/talk2");
            if(!dir.exists()){
            	dir.mkdir();
            }
            File[] pageDirs = dir.listFiles(filter);
            
            int idx = 0;
            String newFile;
            while(true){
            	boolean fileexist = false;
            	newFile = "talk2"+mPeerName+String.valueOf(idx)+".png";
            	for(int i=0; i<pageDirs.length; i++){
            		if(pageDirs[i].getName().equals(newFile)){
            			fileexist = true;
            			break;
            		}
            	}
            	if(!fileexist){
            		FileOutputStream out = null;
            		try {
            			out = new FileOutputStream(newFile);
            			bm.compress(Bitmap.CompressFormat.PNG, 90, out);
            		} catch (Exception e) {
            			e.printStackTrace();
            		}
            		finally{
            			if(out!=null){
            				try {
            					out.close();
            				} catch (IOException e) {
            					// TODO Auto-generated catch block
            					e.printStackTrace();
            				}
            			}
            		}
            	}
            	idx++;
            }
		}
		*/
		int imgWidth = bm.getWidth();
		int imgHeight = bm.getHeight();

		int sw = mRemotePeerView.getWidth();
		int sh = mRemotePeerView.getHeight();

		int x = 0;
		int y = 0;
		if(sw > imgWidth){
			x = (sw - imgWidth)/2;
		}

		if(sh > imgHeight){
			y = (sh - imgHeight)/2;
		}
		
		Canvas c = mRemotePeerView.getHolder().lockCanvas(new Rect(x, y, x+imgWidth, y + imgHeight));
		if(c!=null){
			//sw -= 10;
			//sh -= 8;
			//Bitmap newBmp = Bitmap.createScaledBitmap(bm, sw, sh, false);
			c.drawBitmap(bm, x, y, new Paint());
			mRemotePeerView.getHolder().unlockCanvasAndPost(c);
		}
		bm.recycle();

		mTxInfo1.setText(imgWidth+"x"+imgHeight);
		Log.d(TAG, "Draw Bitmap "+index);
		index++;
	}

	public void destroy(){
		Log.d(TAG, "destroy(), mConfigureChangeRestart="+mConfigureChangeRestart);
		if(mCamera != null && mConfigureChangeRestart == false){
			closeCamera();
		}
	}

	public void surfaceCreated(SurfaceHolder holder) {
		Log.d(TAG, "surfaceCreated");
		//mStartBtn.setEnabled(true);

		if(holder.equals(mCameraPreviewHolder)){
			if(mCamera == null){
				if(openCamera(holder) == false){
					mCamera.release();
					mCamera = null;
				}
			}
			mSurfaceDestroy = false;
		}

	}

	public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {

		if(holder.equals(mCameraPreviewHolder)){
			Log.d(TAG, "surfaceChanged mCamera="+mCamera);
			if(mCamera == null) return;

			startPreview(holder);

			return;
		}
	}



	public void surfaceDestroyed(SurfaceHolder holder) {
		Log.d(TAG, "surfaceDestroyed mConfigureChangeRestart="+mConfigureChangeRestart);

		if(holder.equals(mCameraPreviewHolder)){
			if(mConfigureChangeRestart == true){
				if(mCamera != null){
					mCamera.setPreviewCallback(null);
					mCamera.stopPreview();
				}
			}
			else{
				closeCamera();
			}
			mSurfaceDestroy = true;
		}
	}

	private void startPreview(SurfaceHolder holder){
		Camera.Parameters parameters = mCamera.getParameters();
		
		SharedPreferences settings = mContext.getSharedPreferences(ConfigActivity.TALK_CONFIG_PREFS, 0);
		int prefWidth = settings.getInt(ConfigActivity.VIDEO_WIDTH, ConfigActivity.DEFAULT_VIDEO_WIDTH);
		int prefHeight = settings.getInt(ConfigActivity.VIDEO_HEIGHT, ConfigActivity.DEFAULT_VIDEO_HEIGHT);
		
		Log.d(TAG, "Start preview at "+prefWidth+"x"+prefHeight);
		parameters.setPreviewSize(prefWidth, prefHeight);
		//requestLayout();

		mCamera.setParameters(parameters);
		mCamera.setPreviewCallback(this);
		try {
			mCamera.setPreviewDisplay(holder);
			mCamera.startPreview();
			mStopBtn.setEnabled(true);
		} catch (IOException e) {
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
	}

	private boolean openCamera(SurfaceHolder holder)
	{
		Log.d(TAG, "openCamera: mCamera="+mCamera);
		if(mCamera != null)
			return true;

		mCamera = Camera.open();
		Parameters camParams = mCamera.getParameters();
		// check if the mCamera supports the YCbCr_420_SP format
		List<Integer> pvFmts = camParams.getSupportedPreviewFormats();
		if(pvFmts == null || !pvFmts.contains(PixelFormat.YCbCr_420_SP))
		{
			AlertDialog alertDialog = new AlertDialog.Builder(getContext()).create();
			alertDialog.setTitle("Camera error");
			alertDialog.setMessage("The mCamera of your device is not currently supported...\nSmartCam will now exit.");
			alertDialog.setIcon(android.R.drawable.ic_dialog_alert);
			alertDialog.setButton("OK", new DialogInterface.OnClickListener() {
				public void onClick(DialogInterface dialog, int which)
				{
					//exitApp();
				}
			});
			alertDialog.show();
			return false;
		}

		int frameWidth = IDEAL_FRAME_WIDTH;
		int frameHeight = IDEAL_FRAME_HEIGHT;

		mSupportedPreviewSizes = camParams.getSupportedPreviewSizes();
		mPreviewSize = getOptimalPreviewSize(mSupportedPreviewSizes, 
				mCameraPreview.getWidth(), mCameraPreview.getHeight());

		Log.d(TAG, "mPreviewSize.width="+mPreviewSize.width + " mPreviewSize.height="+mPreviewSize.height);

		//camParams.setPreviewSize(frameWidth, frameHeight);
		camParams.setPreviewFrameRate(15);
		camParams.setPreviewFormat(PixelFormat.YCbCr_420_SP);
		mCamera.setParameters(camParams);

		// compute the frame buffer size
		// allocate the frame buffers and add them to the mCamera

		PixelFormat pixelFmt = new PixelFormat();
		PixelFormat.getPixelFormatInfo(PixelFormat.YCbCr_420_SP, pixelFmt);

		Log.d(TAG, "pixelFmt.bitsPerPixel="+pixelFmt.bitsPerPixel);
		int frameBufferSize = frameWidth * frameHeight * pixelFmt.bitsPerPixel / 8;
		Log.d(TAG, "frameBufferSize="+frameBufferSize);


		return true;
	}


	private PreviewSize getOptimalPreviewSize(List<Size> sizes, int w, int h) {
		SharedPreferences mSettings = getContext().getSharedPreferences(
				ConfigActivity.TALK_CONFIG_PREFS, 0);
		int ww = mSettings.getInt(ConfigActivity.VIDEO_WIDTH, 
				ConfigActivity.DEFAULT_VIDEO_WIDTH);
		int hh = mSettings.getInt(ConfigActivity.VIDEO_HEIGHT, 
				ConfigActivity.DEFAULT_VIDEO_HEIGHT);

		return new PreviewSize(ww, hh);
	}

	private void closeCamera()
	{
		if(mCamera != null)
		{
			Log.d(TAG, "closeCamera()");
			mCamera.setPreviewCallbackWithBuffer(null);
			mCamera.stopPreview();
			mCamera.release();
			mCamera = null;
		}
	}


	public void onPreviewFrame(byte[] data, Camera arg1) {
		//long timestamp = System.currentTimeMillis();
		Log.d(TAG, "onPreviewFrame mSessionID="+mSessionID);

		if(mSessionID != 0 && mLib != null){
			int ret = mLib.postVideoPreviewData(data, mSessionID);
			if(ret != 0){
				mSessionID = 0;
				Message msg = mMsgHandler.obtainMessage(MainUI.MESSAGE_USER_STOP_SESSION);
				msg.obj = null;
				msg.arg1 = mSessionID;
				msg.sendToTarget();
			}
		}
	}

	List<RxFrameInfo> mTxFrameInfo= new ArrayList<RxFrameInfo>(50);
	public void updateTxFrameUI(int rxLength){
		mMsgHandler.removeCallbacks(mUpdateTxRunnable);
		updateStatisticsUI(mTxFrameInfo, rxLength, mTxInfo2, mTxInfo3);
		mMsgHandler.postDelayed(mUpdateTxRunnable, 500);
	}
	Runnable mUpdateTxRunnable = new Runnable(){

		public void run() {
			updateTxFrameUI(0);
		}
	};

	List<RxFrameInfo> mRxFrameInfo= new ArrayList<RxFrameInfo>(50);
	private void updateRxFrameUI(int rxLength){
		mMsgHandler.removeCallbacks(mUpdateRxRunnable);
		updateStatisticsUI(mRxFrameInfo, rxLength, mRxInfo2, mRxInfo3);
		mMsgHandler.postDelayed(mUpdateRxRunnable, 500);
	}

	Runnable mUpdateRxRunnable = new Runnable(){

		public void run() {
			updateRxFrameUI(0);
		}
	};

	private void updateStatisticsUI(List<RxFrameInfo> rxFrameList, int sentBytes,
			TextView infoView1, TextView infoView2){
		int rxLength = 0;
		int nFrames = 0;
		RxFrameInfo fi;

		long txTime = System.currentTimeMillis();

		if(sentBytes > 0){
			fi = new RxFrameInfo();
			fi.size = sentBytes;
			fi.time = txTime;
			rxFrameList.add(0, fi);
		}

		int deleteIndex = rxFrameList.size() - 1;

		for(int i=0; i<rxFrameList.size(); i++){
			fi = rxFrameList.get(i);
			if(txTime - fi.time < 1000){
				rxLength += fi.size;
				nFrames++;
			}
			else{
				deleteIndex = i;
			}
		}

		for(int i = deleteIndex; i < rxFrameList.size() - 1; i++){
			rxFrameList.remove(deleteIndex);
		}

		String info = String.valueOf(nFrames)+"fps";
		if(infoView1 != null)
			infoView1.setText(info);

		if(infoView2 == null)
			return;

		rxLength *= 8;
		if(rxLength < 1000){
			info = (String.valueOf(rxLength)+"bps");
		}
		else{
			float fRxlength = ((float)rxLength)/1000.0f;
			if(fRxlength < 100){
				info = String.format("%1$.2fKbps", fRxlength);
			}
			else if(fRxlength < 1000){
				info = (String.format("%1$.1fKbps", fRxlength));
			}
			else{
				fRxlength = fRxlength/1000;
				if(fRxlength < 10){
					info = (String.format("%1$.3fMbps", fRxlength));
				}
				else{
					info = (String.format("%1$.2fMbps", fRxlength));
				}
			}
		}

		infoView2.setText(info);
	}

	static class RxFrameInfo{
		long time;
		long size;
	}

	public List<Size> getSupportedPreviewSizes(){
		if(mSupportedPreviewSizes == null){
			boolean cameraOpened = false;
			if(mCamera == null){
				mCamera = Camera.open();
				cameraOpened = true;
			}
			Parameters camParams = mCamera.getParameters();
			mSupportedPreviewSizes = camParams.getSupportedPreviewSizes();
			
			if(cameraOpened == true){
				mCamera.release();
				mCamera = null;
			}
		}

		return mSupportedPreviewSizes;
	}

	
	protected void onFinishInflate (){
		super.onFinishInflate();
		Log.d(TAG, "onFinishInflate");
	}

}
