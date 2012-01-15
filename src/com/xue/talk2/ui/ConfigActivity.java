package com.xue.talk2.ui;

import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.os.Bundle;
import android.preference.ListPreference;
import android.preference.Preference;
import android.preference.PreferenceActivity;
import android.preference.PreferenceScreen;
import android.util.Log;
import android.widget.Toast;

public class ConfigActivity extends PreferenceActivity implements OnSharedPreferenceChangeListener{
	static final String TALK_CONFIG_PREFS = "talk_config_prefs";
	

	static final String USER_NAME_KEY = "pref_my_name";
	static final String DEFAULT_USER_NAME = "your_name";
	static final String BROADCAST_PORT_KEY = "pref_broadcast_port";
	static final String DEFAULT_BROADCAST_PORT = "19361";
	static final String VIDEO_RES_KEY="video_res_pref";
	
	static final String VIDEO_WIDTH="vwidth";
	static final String VIDEO_HEIGHT="vheight";
	static final int DEFAULT_VIDEO_WIDTH = 320;
	static final int DEFAULT_VIDEO_HEIGHT = 240;
	
	private int[] mVideoSizeArray;
	
	private Preference mMynamePref;
	private Preference mChatPortPref;
	private ListPreference mVideoResPref;

	private SharedPreferences mSettings;

	private int mPrefWidth, mPrefHeight;
	private int mResult;


	private static final String TAG="ConfigActivity";
	
	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		mResult = 0;
		addPreferencesFromResource(R.xml.talk2_preference);

		mSettings = getPreferenceScreen().getSharedPreferences();
		
		String preferencesName = this.getPreferenceManager().getSharedPreferencesName();
		Log.d(TAG, "preferencesName="+preferencesName);
	
		mMynamePref = findPreference(USER_NAME_KEY);
		mChatPortPref = findPreference(BROADCAST_PORT_KEY);
		mVideoResPref = (ListPreference)findPreference(VIDEO_RES_KEY);
		
		
		String myname = mSettings.getString(USER_NAME_KEY, DEFAULT_USER_NAME);
		mMynamePref.setSummary(myname);
		String port = mSettings.getString(BROADCAST_PORT_KEY, DEFAULT_BROADCAST_PORT);
		mChatPortPref.setSummary(port);
		
		Intent intent = getIntent();
		mVideoSizeArray = intent.getIntArrayExtra("videoSize");

		mPrefWidth = mSettings.getInt(VIDEO_WIDTH, DEFAULT_VIDEO_WIDTH);
		mPrefHeight = mSettings.getInt(VIDEO_HEIGHT, DEFAULT_VIDEO_HEIGHT);
		
		int selectedPos = 0;
		
		String[] videoSizeStringArray = null;
		String[] videoSizeArrayValues = null;
		
		if(mVideoSizeArray != null && mVideoSizeArray.length > 0){
			videoSizeStringArray = new String[mVideoSizeArray.length/2];
			videoSizeArrayValues = new String[videoSizeStringArray.length];
			int j = 0;
			for(int i=0; i<mVideoSizeArray.length; i+=2){
				videoSizeStringArray[j] =  mVideoSizeArray[i] + " x " +  mVideoSizeArray[i+1];
				videoSizeArrayValues[j++] = String.valueOf(i);

				if(mVideoSizeArray[i] == mPrefWidth){
					selectedPos = i/2;
				}

				if(mVideoSizeArray[i] == mPrefWidth && mVideoSizeArray[i+1] == mPrefHeight){
					selectedPos = i/2;
				}
			}
		}
		else{
			videoSizeStringArray = new String[1];
			videoSizeArrayValues = new String[videoSizeStringArray.length];
			videoSizeStringArray[0] = mPrefWidth + " x " + mPrefHeight;
			videoSizeArrayValues[0] = "0";
		}
		
		mVideoResPref.setEntries(videoSizeStringArray);
		mVideoResPref.setEntryValues(videoSizeArrayValues);
		mVideoResPref.setSummary(mPrefWidth+"x"+mPrefHeight);
		mVideoResPref.setValueIndex(selectedPos);
		setResult(mResult);
	}
	
	protected void onResume(){
		super.onResume();
		mSettings.registerOnSharedPreferenceChangeListener(this);
	}
	
	protected void onPause() {
        super.onPause();
        mSettings.unregisterOnSharedPreferenceChangeListener(this);
    }

	
	public boolean onPreferenceTreeClick(PreferenceScreen preferenceScreen,
			Preference preference) {
		SharedPreferences share = preference.getSharedPreferences();
        String str = share.getString(preference.getKey(), "");
        Toast.makeText(this, str, Toast.LENGTH_LONG).show();
        return false;
	}

	@Override
	public void onSharedPreferenceChanged(SharedPreferences pref, String key) {
		if (key.equals(VIDEO_RES_KEY)) {
			int prefIndex = Integer.valueOf(pref.getString(key, "0"));
			
			SharedPreferences.Editor editor = mSettings.edit();
			int vwidth = mVideoSizeArray[prefIndex];
			int vheight = mVideoSizeArray[prefIndex+1];
			if(vwidth !=  mPrefWidth || vheight != mPrefHeight){
				editor.putInt(VIDEO_WIDTH, vwidth);
				editor.putInt(VIDEO_HEIGHT, vheight);
				mPrefWidth = vwidth;
				mPrefHeight = vheight;
				mVideoResPref.setSummary(mPrefWidth+"x"+mPrefHeight);
				mResult |= 0x4;
				editor.commit();
			}
			setResult(mResult);
			//Toast.makeText(this, vwidth+"x"+vheight, Toast.LENGTH_LONG).show();
		}
		if (key.equals(USER_NAME_KEY)) {
			String myname = pref.getString(key, DEFAULT_USER_NAME);
			mMynamePref.setSummary(myname);
			mResult |= 0x1;
			setResult(mResult);
		}
	}
}

/*	static final String TALK_CONFIG_PREFS = "talk_config_prefs";
	static final String USER_NAME = "user_name";
	static final String DEFAULT_USER_NAME = "your_name";
	static final String BROADCAST_PORT = "BROADCAST_PORT";
	static final String VIDEO_WIDTH="vwidth";
	static final String VIDEO_HEIGHT="vheight";

	static final int DEFAULT_VIDEO_WIDTH = 320;
	static final int DEFAULT_VIDEO_HEIGHT = 240;

	
	static final int DEFAULT_BROADCAST_PORT = 15353;

	private String mMyName;
	private int mPort;
	private int mPrefWidth, mPrefHeight;

	private SharedPreferences mSettings;
	private static final String TAG="ConfigActivity";

	private int[] mVideoSizeArray;
	private ListView mListView;
	private int mSelectedPos = 0;

	public void onCreate(Bundle savedInstanceState) {
		super.onCreate(savedInstanceState);
		requestWindowFeature(Window.FEATURE_NO_TITLE); 

		mSettings = getSharedPreferences(TALK_CONFIG_PREFS, 0);

		mPrefWidth = mSettings.getInt(VIDEO_WIDTH, DEFAULT_VIDEO_WIDTH);
		mPrefHeight = mSettings.getInt(VIDEO_HEIGHT, DEFAULT_VIDEO_HEIGHT);

		Intent intent = getIntent();
		mVideoSizeArray = intent.getIntArrayExtra("videoSize");

		String[] videoSizeStringArray = null;
		if(mVideoSizeArray != null && mVideoSizeArray.length > 0){
			videoSizeStringArray = new String[mVideoSizeArray.length/2];
			int j = 0;
			for(int i=0; i<mVideoSizeArray.length; i+=2){
				videoSizeStringArray[j++] =  mVideoSizeArray[i] + " x " +  mVideoSizeArray[i+1];

				if(mVideoSizeArray[i] == mPrefWidth){
					mSelectedPos = i/2;
				}

				if(mVideoSizeArray[i] == mPrefWidth && mVideoSizeArray[i+1] == mPrefHeight){
					mSelectedPos = i/2;
				}
			}
		}
		else{
			videoSizeStringArray = new String[1];
			videoSizeStringArray[0] = mPrefWidth + " x " + mPrefHeight;
			mSelectedPos = 0;
		}

		mMyName = mSettings.getString(USER_NAME, USER_NAME);
		Log.d(TAG, "Read username=" + mMyName);

		mPort = mSettings.getInt(BROADCAST_PORT, DEFAULT_BROADCAST_PORT);
		Log.d(TAG, "Read port=" + mPort);

		LayoutInflater inflater = (LayoutInflater)getSystemService(Context.LAYOUT_INFLATER_SERVICE); 
		View vPopupWindow=inflater.inflate(R.layout.config_activity, null);

		final EditText edtUsername=(EditText)vPopupWindow.findViewById(R.id.username_edit); 
		edtUsername.setText(mMyName); 
		final EditText edtPassword=(EditText)vPopupWindow.findViewById(R.id.password_edit);
		edtPassword.setText(String.valueOf(mPort));

		mListView = (ListView)vPopupWindow.findViewById(R.id.camera_preview_size_list);

		mListView.setAdapter(new ArrayAdapter<String>(this, 
				android.R.layout.simple_list_item_checked, 
				videoSizeStringArray));


		mListView.setOnItemClickListener(new OnItemClickListener() {  

			public void onItemClick(AdapterView<?> arg0, View view, int arg2, long arg3) {  
				Log.d(TAG, "arg2="+arg2+" arg3="+arg3);
				mSelectedPos = arg2;
			}  
		});  

		Button btnCancel=(Button)vPopupWindow.findViewById(R.id.BtnCancel); 
		btnCancel.setOnClickListener(new OnClickListener(){ 
			public void onClick(View v) {
				//launchBroadcastThreadInNeeded();
				finish();
			} 
		});

		Button btnOK=(Button)vPopupWindow.findViewById(R.id.BtnOK); 
		btnOK.setOnClickListener(new OnClickListener(){ 
			public void onClick(View v) { 

				String usr = edtUsername.getText().toString();
				usr.trim();
				String port_str = edtPassword.getText().toString();

				int myNum = DEFAULT_BROADCAST_PORT;

				try {
					myNum = Integer.parseInt(port_str.trim());
				} catch(NumberFormatException nfe) {
					Log.e(TAG, "Could not parse " + nfe);
				} 

				if(mVideoSizeArray!=null){
					int vwidth = mVideoSizeArray[mSelectedPos * 2];
					int vheight = mVideoSizeArray[mSelectedPos * 2 + 1];
					int result = 0;

					SharedPreferences.Editor editor = mSettings.edit();
					
					Log.d(TAG, "User select prefer video size:"+ vwidth + " x "+vheight);
					if(mMyName.equals(usr) != true){
						editor.putString(USER_NAME, usr.trim());
						result |= 0x1;
					}
					if( myNum != mPort){
						editor.putInt(BROADCAST_PORT, myNum);
						result |= 0x2;
					}
					if(vwidth !=  mPrefWidth || vheight != mPrefHeight){
						editor.putInt(VIDEO_WIDTH, vwidth);
						editor.putInt(VIDEO_HEIGHT, vheight);
						result |= 0x4;
					}
					
					if(result != 0)
						editor.commit();
					
					setResult(result);
				}
				//launchBroadcastThreadInNeeded();
				finish();
			} 
		}); 

		setContentView(vPopupWindow);
		mListView.setItemChecked(mSelectedPos, true);
		mListView.requestFocus();
	}
}*/
