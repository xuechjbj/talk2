package com.xue.talk2.ui;


import java.util.List;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.ProgressDialog;
import android.content.DialogInterface;
import android.content.Intent;
import android.content.SharedPreferences;
import android.graphics.Bitmap;
import android.hardware.Camera.Size;
import android.os.Bundle;
import android.os.Handler;
import android.os.Message;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.KeyEvent;
import android.view.Menu;
import android.view.MenuInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;

import com.xue.talk2.ui.UserListView.UserInfo;

public class MainUI extends Activity {
	private static final String TAG = "Talk2";
	public static final int DEFAULT_INET_PORT = 19361;
	
	private static final int VIDEO = 0x1;
	private static final int AUDIO = 0x2;

	static final int CHAT_ACTIVITY = 102;
	static final int CONFIG_ACTIVITY = 101;
	static final int Config_Change = 100;

	class SaveState{
		Talk2Lib nativeLib;

		boolean connectDlgShow;
		boolean acceptConfirmDlgShow;
		String connectDlgStr;
		String peerName;
		int sessionID;

		Object userListViewSaved;
		Object videochatViewSaved;
		Object gallerySaved;
	}

	private final String[] mLabelArray = {"View1", "View2"};

	private Talk2Lib mNativeLib = null;

	
	private SharedPreferences mSettings;
	private boolean mConfigureChangeRestart;
	private VideoChatView mVideoChatView;
	private UserListView mUserListView;

	private ProgressDialog mConnectDialog;
	private String mConnectDialogStr;
	private int mSessionID = -1;
	private String mPeerName, mPeerIPAddr;

	private FlingGallery mGallery;
	private boolean mAcceptConfirmDlgShow;
	private boolean mConnectDlgShow;

	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		
		
		Log.d(TAG, "onCreate mNativeLib="+mNativeLib);

		mConnectDialogStr = null;
		requestWindowFeature(Window.FEATURE_NO_TITLE); 
		setContentView(R.layout.main);

		mSettings = PreferenceManager.getDefaultSharedPreferences(this);
		
		mVideoChatView = new VideoChatView(this, mUIHandler);
		mUserListView = new UserListView(this, mUIHandler);

		mGallery = new FlingGallery(this,
				new ArrayAdapter<String>(getApplicationContext(), 
						android.R.layout.simple_list_item_1, mLabelArray){

			public View getView(int position, View convertView, ViewGroup parent)
			{
				if(position == 0){
					return mUserListView;
				}
				else{
					return mVideoChatView;
				}
			}
		});

		mGallery.setPaddingWidth(5);
		mGallery.setIsGalleryCircular(false);

		LinearLayout mlayout = new LinearLayout(this);
		mlayout.setOrientation(LinearLayout.VERTICAL);

		LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
				LinearLayout.LayoutParams.MATCH_PARENT,
				LinearLayout.LayoutParams.MATCH_PARENT);

		layoutParams.setMargins(2, 2, 2, 2);
		layoutParams.weight = 1.0f;

		mlayout.addView(mGallery, layoutParams);
		setContentView(mlayout);	

		mConfigureChangeRestart = false;
		registerForContextMenu(mUserListView);
	}


	public void onRestart(){
		Log.d(TAG, "onRestart");
		super.onRestart();
	}

	
	public void onStart(){
		super.onStart();	
		Object o = getLastNonConfigurationInstance();
		
		Log.d(TAG, "onStart mNativeLib="+mNativeLib+" savedState="+o);
		
		String myname = mSettings.getString(ConfigActivity.USER_NAME_KEY, 
				ConfigActivity.DEFAULT_USER_NAME);
		//mPort = mSettings.getInt(ConfigActivity.BROADCAST_PORT, 
		//		ConfigActivity.DEFAULT_BROADCAST_PORT);
		
		if( o == null){
			if(mNativeLib == null){
				mNativeLib = new Talk2Lib(mUIHandler, this);
				mNativeLib.run(DEFAULT_INET_PORT, myname);
			}
			else if(mNativeLib.getSessionStatus(mSessionID) == 0){
				mGallery.movePrevious();
				mSessionID = -1;
				mNativeLib.setSessionID(-1);
			}
			mVideoChatView.setTalk2Lib(mNativeLib);
			mNativeLib.setHandler(mUIHandler, this);
		}
		else{
			SaveState saved = (SaveState)o;
			mNativeLib = saved.nativeLib;
			mNativeLib.setHandler(mUIHandler, this);

			mVideoChatView.setTalk2Lib(mNativeLib);
			mVideoChatView.setSavedState(saved.videochatViewSaved);
			mUserListView.setSavedState(saved.userListViewSaved);
			mGallery.setSavedState(saved.gallerySaved);
			
			mConnectDialogStr = saved.connectDlgStr;
			mSessionID = saved.sessionID;
			
			if(saved.acceptConfirmDlgShow){
				mPeerName = saved.peerName;
				showConfirmAcceptCall();
			}
			if(saved.connectDlgShow){
				mConnectDialogStr = saved.connectDlgStr;
				Log.d(TAG," onCreate(), mConnectDialog="+mConnectDialog);
				showConnectProgressDialog();
				Log.d(TAG," onCreate(), mConnectDialog="+mConnectDialog);
			}
		}
		
	}

	public void onResume(){
		super.onResume();
		Log.d(TAG, "onResume(), mConnectDialog="+mConnectDialog+" this="+this);

		//this.getWindow().setType(WindowManager.LayoutParams.TYPE_KEYGUARD);
		/*mGallery.moveNext();
		mVideoChatView.startPlay(1);
		mNativeLib.testCodec();*/
	}

	public void onAttachedToWindow() {
		//this.getWindow().setType(WindowManager.LayoutParams.TYPE_KEYGUARD);
		super.onAttachedToWindow();
	}
	
	public void onPause(){
		Log.d(TAG, "===================>onPause(), isFinishing="+this.isFinishing());
		super.onPause();
	}
	

	/** Called at the end of the visible lifetime. */
	public void onStop(){
		Log.d(TAG, "onStop(), isFinishing="+this.isFinishing());
		super.onStop();
		mNativeLib.setHandler(null, null);
	}

	public Object onRetainNonConfigurationInstance (){
		SaveState saved = new SaveState();
		Log.d(TAG, "onRetain:mConnectDlgShow="+mConnectDlgShow);

		saved.connectDlgShow = mConnectDlgShow;
		if(mConnectDlgShow){
			Log.d(TAG, "mConnectDialog dimiss="+mConnectDialog);
			saved.connectDlgStr = mConnectDialogStr;
		}

		saved.acceptConfirmDlgShow = mAcceptConfirmDlgShow;
		if(mAcceptConfirmDlgShow){
			saved.peerName = mPeerName;
		}

		saved.nativeLib = mNativeLib;

		saved.sessionID = mSessionID;
		saved.gallerySaved = mGallery.onRetainNonConfigurationInstance();
		saved.videochatViewSaved = mVideoChatView.onRetainNonConfigurationInstance();
		saved.userListViewSaved = mUserListView.onRetainNonConfigurationInstance();
		mConfigureChangeRestart = true;

		return saved;	
	}
	
	public void onDestroy(){
		super.onDestroy();
		Log.d(TAG, "onDestroy mConfigureChangeRestart="+mConfigureChangeRestart);

		if(mConfigureChangeRestart == false){
			mNativeLib.destroy();
			mVideoChatView.destroy();
			//mUserListView.destroy();
			//System.exit(0);  
			// 或者下面这种方式  
			// android.os.Process.killProcess(android.os.Process.myPid());
		}

		if(mConnectDialog != null){
			mConnectDialog.dismiss();
			mConnectDialog = null;
		}
	}

	protected void onActivityResult (int requestCode, int resultCode, Intent data){
		if(CONFIG_ACTIVITY == requestCode){
			Log.d(TAG, "onActivityResult CONFIG_ACTIVITY return="+resultCode);
			if((resultCode & 0x01) == 0x01){
				String myname = mSettings.getString(ConfigActivity.USER_NAME_KEY, 
						ConfigActivity.DEFAULT_USER_NAME);
				Log.d(TAG, "My new name is "+myname);
				mNativeLib.setMyname(myname);
			}
		}
	}
	
	public boolean onCreateOptionsMenu(Menu menu) {
		super.onCreateOptionsMenu(menu);

		if(mSessionID == -1){
			MenuInflater inflater = getMenuInflater();
			inflater.inflate(R.menu.userlist_option, menu);
			return true;
		}
		
		return false;
	}
	
	public boolean onOptionsItemSelected(MenuItem item) {
        switch (item.getItemId()) {
        case R.id.settings:
        	mUserListView.showConfigureWindow();
            return true;
        default:
            Log.e(TAG, "Unknown menu item selected");
            return false;
        }
    }
	
	public boolean onKeyDown(int keyCode, KeyEvent event) {

		if (keyCode == KeyEvent.KEYCODE_BACK) {
			Log.d(TAG, "Back key was pressed mSessionID="+mSessionID);
			if(mSessionID != -1){
				showConfirmExitDialog();
				return true;
			}
		}

		if (keyCode == KeyEvent.KEYCODE_HOME) {
			Log.d(TAG, "Home key was pressed");
			//if(mSessionID != 0)
			//	return true;
		}
		return super.onKeyDown(keyCode, event);    
	}

	public boolean onContextItemSelected(MenuItem item) {	
		return mUserListView.onContextItemSelected(item);
	}
	
	void connectInet(UserInfo user){
		String inetServerAddr = user.ipAddr;
		String inetServerPort = String.valueOf(DEFAULT_INET_PORT);

		String fmt = this.getText(R.string.connecting_str).toString();
		mConnectDialogStr = String.format(fmt, inetServerAddr, inetServerPort);
		Log.d(TAG, "connectStr="+mConnectDialogStr);

		showConnectProgressDialog();

		int width = mSettings.getInt(ConfigActivity.VIDEO_WIDTH, ConfigActivity.DEFAULT_VIDEO_WIDTH);
		int height = mSettings.getInt(ConfigActivity.VIDEO_HEIGHT, ConfigActivity.DEFAULT_VIDEO_HEIGHT);
		
		user.sessionID = mSessionID = mNativeLib.call(inetServerAddr, DEFAULT_INET_PORT, 
				VIDEO, VIDEO, width, height);
	}

	private void showConnectProgressDialog(){
		mConnectDialog = new ProgressDialog(this);
		mConnectDialog.setProgressStyle(ProgressDialog.STYLE_SPINNER);

		//mConnectDialog.setProgressStyle(ProgressDialog.STYLE_HORIZONTAL);  
		//progressDialog.setIcon(R.drawable.alert_dialog_icon);  
		mConnectDialog.setMessage(mConnectDialogStr);  
		mConnectDialog.setCancelable(false);
		mConnectDialog.setButton(getString(android.R.string.cancel), 
				new DialogInterface.OnClickListener(){  

			public void onClick(DialogInterface dialog, int which) {  

				dialog.cancel();
				mConnectDialog = null;
				mConnectDialogStr = null;
				mNativeLib.hangoffCall(mSessionID);

				UserInfo user = mUserListView.findUserBySessionID(mSessionID);
				mSessionID = -1;
				if(user == null){
					return;
				}
				user.sessionID = -1;
				mConnectDlgShow = false;
			}  
		});

		mConnectDlgShow = true;
		mConnectDialog.show();
		return;
	}


	static final int MESSAGE_CONFIRM_ACCEPT = 1000;
	static final int MESSAGE_USER_DETECTED = 1001;
	static final int MESSAGE_USER_STOP_SESSION = 1002;
	static final int MESSAGE_START_PLAY = 1003;
	static final int MESSAGE_CONNECTED_REJECT = 1004;
	static final int MESSAGE_INVITEE_RESP = 1005;
	static final int MESSAGE_SHOW_BITMAP = 1006;
	static final int MESSAGE_TX_LENGTH = 1007;
	static final int MESSAGE_EXIT_APP = 1008;
    static final int MESSAGE_PERIODIC_TIMER = 1009;
    static final int MESSAGE_CALL_SESSION_BEGIN = 1010;


	static final int PEER_OK = 1;
	static final int PEER_REJECT = 2;

	private Handler mUIHandler = new  Handler()
	{
		public void handleMessage(Message msg)
		{
			UserInfo user;
			
			switch(msg.what)
			{
			case MESSAGE_CALL_SESSION_BEGIN:
				String ipStr = (String)msg.obj;
				UserInfo u = mUserListView.findUserByIp(ipStr);
				Log.d(TAG, "Call begin for ip:"+ipStr+", sessionID:"+msg.arg1);
				if(u == null){
					Log.e(TAG, "Can't find user info for ip;"+ipStr);
					return;
				}
				mSessionID = msg.arg1;
				u.sessionID = msg.arg1;
				Log.d(TAG, "user="+u);
				break;
			case MESSAGE_PERIODIC_TIMER:
				mUserListView.handleTimeEvent();
				break;
			case MESSAGE_USER_DETECTED:
				mUserListView.handlePeerBroadcast(msg.obj);
				break;
			case MESSAGE_CONFIRM_ACCEPT:
				user = (UserInfo)msg.obj;
				Log.d(TAG, "Receiving confirm accept call from user:"+user.name+" ip="+user.ipAddr);
				mPeerName =  user.name;
				mPeerIPAddr = user.ipAddr;
				mSessionID = msg.arg1;
				mUserListView.addUserForce(user);
				showConfirmAcceptCall();
				break;
			case MESSAGE_INVITEE_RESP:
				handleInviteResp(msg);
				break;
			case MESSAGE_START_PLAY:
				mNativeLib.startPlayVideo(msg.arg1, msg.arg2);
				break;

			case MESSAGE_SHOW_BITMAP:
				handleBitmap(msg);
				break;
			case MESSAGE_EXIT_APP:
				finish(); //fall down
			case MESSAGE_USER_STOP_SESSION:
				Log.d(TAG, "MESSAGE_USER_STOP_SESSION = "+msg.arg1);
				if(mConfirmCallDialog != null){
					mConfirmCallDialog.dismiss();
					mConfirmCallDialog = null;
					mAcceptConfirmDlgShow = false;
				}
				
				int id = msg.arg1;
				user = mUserListView.findUserBySessionID(id);
				if(user == null){
					Log.e(TAG, "Unknow session id "+id+ " in user list");
					return;
				}
				
				mGallery.movePrevious();
				mVideoChatView.destroy();

				
				user.sessionID = -1;
				mSessionID = -1;
				mNativeLib.setSessionID(mSessionID);
				mNativeLib.hangoffCall(id);
				break;
				
			case MESSAGE_TX_LENGTH:
				mVideoChatView.updateTxFrameUI(msg.arg1);
				break;
			}
		}
	};

	private void handleInviteResp(Message msg){
		Log.d(TAG, "Peer resp code="+msg.arg1+" mConnectDialog="+mConnectDialog);
		if(msg.arg1 == PEER_REJECT || msg.arg1 == PEER_OK){
			if(mConnectDialog != null){
				mConnectDialog.dismiss();
				mConnectDialog = null;
				mConnectDialogStr = null;
				mConnectDlgShow = false;
			}

			if(msg.arg1 == PEER_REJECT){
				Log.d(TAG, "Peer reject call");
				showRejectDialog(msg.arg2);
			}
			else if(msg.arg1 == PEER_OK){
				Log.d(TAG, "Peer accept call");
				if(mSessionID != msg.arg2){
					Log.d(TAG, "user stop "+msg.arg2+" eariler");
					mNativeLib.setSessionID(mSessionID);
					mNativeLib.hangoffCall(msg.arg2);
					return;
				}

				mGallery.moveNext();
				mSessionID = msg.arg2;
				
				String invitee_name = (String)(msg.obj);
				Log.d(TAG, "invitee_name="+invitee_name+" mSessionID="+mSessionID);
				UserInfo user = mUserListView.findUserBySessionID(mSessionID);
				user.name = invitee_name;
				mUserListView.refreshList();
				
				mNativeLib.setSessionID(mSessionID);
				mVideoChatView.startPlay(mSessionID, invitee_name);
			}
		}
	}

	AlertDialog mConfirmCallDialog = null;
	private void showConfirmAcceptCall(){

		mConfirmCallDialog = new AlertDialog.Builder(MainUI.this).create();
		mConfirmCallDialog.setTitle("Confirm");

		String fmt = MainUI.this.getText(R.string.confirm_accept).toString();
		String acceptStr = String.format(fmt, mPeerName, "");

		mConfirmCallDialog.setMessage(acceptStr);
		mConfirmCallDialog.setIcon(android.R.drawable.ic_dialog_alert);

		mConfirmCallDialog.setButton(DialogInterface.BUTTON_POSITIVE, getText(android.R.string.ok), 
				new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which){
				int width = mSettings.getInt(ConfigActivity.VIDEO_WIDTH, ConfigActivity.DEFAULT_VIDEO_WIDTH);
				int height = mSettings.getInt(ConfigActivity.VIDEO_HEIGHT, ConfigActivity.DEFAULT_VIDEO_HEIGHT);
				
				mNativeLib.acceptCall(true, mSessionID, width, height);
				mGallery.moveNext();

				mVideoChatView.startPlay(mSessionID, mPeerName);
				mNativeLib.setSessionID(mSessionID);
				mAcceptConfirmDlgShow = false;
			}
		});
		
		
		mConfirmCallDialog.setButton(DialogInterface.BUTTON_NEGATIVE, getText(android.R.string.cancel),
				new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which){
				mNativeLib.acceptCall(false, mSessionID, 0, 0);
				mAcceptConfirmDlgShow = false;
				mSessionID = -1;
			}
		});
		mConfirmCallDialog.show();
		mAcceptConfirmDlgShow = true;
	}

	private void handleBitmap(Message msg){
		mVideoChatView.showBitmapStream((Bitmap)msg.obj, msg.arg1);
	}

	private void showRejectDialog(int id){
		AlertDialog alertDialog;

		alertDialog = new AlertDialog.Builder(MainUI.this).create();
		alertDialog.setTitle("Info");

		UserInfo user = mUserListView.findUserBySessionID(id);
		if(user == null){
			Log.e(TAG, "Unknow session id "+id+ " in user list");
			return;
		}
		String who = user.name;
		String fmt = MainUI.this.getText(R.string.reject_connected_str).toString();
		String connectStr = String.format(fmt, who);

		alertDialog.setMessage(connectStr);
		alertDialog.setIcon(android.R.drawable.ic_dialog_alert);
		alertDialog.setButton("OK", new DialogInterface.OnClickListener() {
			public void onClick(DialogInterface dialog, int which){

			}
		});
		alertDialog.show();
	}


	public void showConfirmExitDialog(){
		AlertDialog.Builder builder = new AlertDialog.Builder(this); 
		builder.setMessage("Are you sure you want to stop chat?") 
		.setCancelable(false) 
		.setPositiveButton(android.R.string.yes, new DialogInterface.OnClickListener() { 
			public void onClick(DialogInterface dialog, int id) { 
				Message msg = mUIHandler.obtainMessage(MainUI.MESSAGE_EXIT_APP);
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
	
	List<Size> getSupportedVideoSizes(){
		return mVideoChatView != null?mVideoChatView.getSupportedPreviewSizes():null;
	}
}