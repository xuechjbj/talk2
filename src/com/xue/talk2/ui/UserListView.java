package com.xue.talk2.ui;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;

import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.hardware.Camera.Size;
import android.os.Handler;
import android.text.InputType;
import android.text.Spanned;
import android.text.method.NumberKeyListener;
import android.util.Log;
import android.view.ContextMenu;
import android.view.ContextMenu.ContextMenuInfo;
import android.view.KeyEvent;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.InputMethodManager;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.BaseAdapter;
import android.widget.EditText;
import android.widget.ImageButton;
import android.widget.ImageView;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;


public class UserListView extends LinearLayout implements FlingGallery.FlingableView{

	private ListView mListView;
	private String TAG="UserListView";
	private MainUI mMainUI;
	private static int sIndex = 0;

	static final int MAX_VIDEO_WIDTH = 500;
	static final int MAX_VIDEO_HEIGHT = 500;
	//private Handler mUIHandler;

	class SavedState{
		List<UserInfo> userList;
	}

	private SavedState mLastStates;
	SharedPreferences mSettings;
	private ImageButton mConnectBtn;

	public static class UserInfo{
		public UserInfo(){
			name = "";
			description = "";
			ipAddr = "";
			beat = 10;
			sessionID = -1;
		}
		
		public String name;
		public String description;
		public String ipAddr;
		public int sessionID;
		public int beat;
		public boolean bOnTalk;
		public int type;
		
		public boolean isSame(UserInfo u){
			return u.name==name && u.description==description && u.ipAddr==ipAddr;
		}
	}

	private MyCustomAdapter mListAdapter = new MyCustomAdapter();
	
	 public UserListView(MainUI context, Handler handler) {
		super(context);
		Log.d(TAG, "constructor");
		
		mMainUI = context;
		//mUIHandler = handler;

		LayoutInflater layoutInflater = (LayoutInflater) context.getSystemService(Context.LAYOUT_INFLATER_SERVICE);

		ViewGroup mainView = (ViewGroup) layoutInflater.inflate(R.layout.userlistview, null);

		((ImageButton)mainView.findViewById(R.id.config_btn)).setOnClickListener(mOnConfigBtnClick);

		mListView = (ListView)mainView.findViewById(R.id.lv_main);

		mListView.setAdapter(mListAdapter);

		mListView.setOnItemClickListener(new OnItemClickListener() {  

			public void onItemClick(AdapterView<?> arg0, View view, int arg2, long arg3) {  
				Log.d(TAG, "arg2="+arg2+" arg3"+arg3);

				UserInfo user = mListAdapter.mUserList.get(arg2);

				mMainUI.connectInet(user);
			}  
		});
		
		mListView.setOnCreateContextMenuListener(new OnCreateContextMenuListener() {

			@Override
			public void onCreateContextMenu(ContextMenu menu, View arg1,
					ContextMenuInfo arg2) {
				Log.d(TAG, "onCreateContextMenu ");
				//menu.setHeaderTitle(R.string.contentMenu); 
				menu.add("Save");
				menu.add("Delete");
			} 
			
		});
		
		/*list.setOnItemLongClickListener(new OnItemLongClickListener(){

			@Override
			public boolean onItemLongClick(AdapterView<?> arg0, View arg1,
					int arg2, long arg3) {
				//mListView.showContextMenu();
				return true;
			}
			
		});*/
		
		

		LinearLayout.LayoutParams layoutParams = new LinearLayout.LayoutParams(
				LinearLayout.LayoutParams.MATCH_PARENT,
				LinearLayout.LayoutParams.MATCH_PARENT);

		this.addView(mainView, layoutParams);
		
		
		final EditText edAddress = (EditText) mainView.findViewById(R.id.user_input_ip);
		edAddress.setKeyListener(new IPAddressKeyListener());
		edAddress.clearFocus();
		
		mConnectBtn = (ImageButton) (mainView.findViewById(R.id.connect_btn));
		mConnectBtn.setEnabled(false);
		
		mConnectBtn.setOnClickListener(new OnClickListener(){

			@Override
			public void onClick(View arg0) {
				UserInfo user = new UserInfo();
				user.name = "myfriend"+sIndex;
				user.ipAddr = edAddress.getText().toString();
				edAddress.clearFocus();
				sIndex++;
				
				mListAdapter.addUserForce(user);
				
				InputMethodManager imm = (InputMethodManager) mMainUI.getSystemService(
						mMainUI.INPUT_METHOD_SERVICE);
				imm.hideSoftInputFromWindow(getWindowToken(), 0);
				mMainUI.connectInet(user);
			}
			
		});
	}
	
	public boolean onContextItemSelected(MenuItem item) {
		
		AdapterView.AdapterContextMenuInfo menuInfo;
        menuInfo =(AdapterView.AdapterContextMenuInfo)item.getMenuInfo();
        
        Log.d(TAG, "onContextItemSelected "+item.getTitle()+" selected pos="+menuInfo.position);
        
		if(item.getTitle()=="Save"){
			Log.d(TAG, "Save memu was selected");
			UserInfo u = mListAdapter.mUserList.get(menuInfo.position);
			saveUserInfo(u);
		}
		else if(item.getTitle()=="Action 2"){

		}
		else {
			return false;
		}
		return true;
	}

	private static final String TALK_USER_INFO = "talk_user_info";
	void saveUserInfo(UserInfo user){
	
		File f = mMainUI.getFilesDir();
		String root = f.getPath();
		
		if(user == null){
			Log.d(TAG, "saveUserInfo user=null");
			return;
		}
	
		if(user.ipAddr == null || user.ipAddr.length() == 0){
			Log.d(TAG, "Can't save ipAddr=0 for user="+user.name);
			return;
		}
		
		File newDir = new File(root +"/"+TALK_USER_INFO);
		if(!newDir.exists()){
			newDir.mkdir();
		}
		
		File newUserInfo = new File(newDir.getPath() + "/" + user.ipAddr+".userinfo");
		
        Log.d(TAG, "Create new UserInfo save:"+newUserInfo.getAbsolutePath());

        DataOutputStream out = null;
        try{
        	newUserInfo.createNewFile();
        	out = new DataOutputStream(new FileOutputStream(newUserInfo));
        	byte[] ip = user.ipAddr.getBytes("utf-8");
        	out.writeInt(ip.length);
        	out.write(ip, 0, ip.length);

        	if(user.name != null){
        		byte[] name = user.name.getBytes("utf-8");
        		out.writeInt(name.length);
        		out.write(name, 0, name.length);
        	}
        	else{
        		out.write(0);
        	}
        }
        catch(Exception e){
        	e.printStackTrace();
        }
        finally{
        	if (out != null) {
                try {
                    out.close();
                } catch (java.io.IOException e) {}
            }
        }
	}
	
	void readUserInfo(File fUserInfo, UserInfo user){
		if(fUserInfo.exists() && fUserInfo.canRead()){
			DataInputStream in = null;
            try{
                in = new DataInputStream(new FileInputStream(fUserInfo));
                int ipLen = in.readInt();
                if(ipLen>0 && ipLen<=512){
                    byte[] ip = new byte[ipLen];
                    in.read(ip);
                    user.ipAddr = new String(ip, 0, ip.length, "utf-8");
                    Log.d(TAG, "read ipAddr="+user.ipAddr);
                }

                int nameLen = in.readInt();
                if(nameLen>0 && nameLen<=512){
                    byte[] name = new byte[nameLen];
                    in.read(name);
                    user.name = new String(name, 0, name.length, "utf-8");
                    Log.d(TAG, "read name="+user.name);
                }
                else{
                	user.name = "Unknown";
                }
            }
            catch(Exception e){
            	e.printStackTrace();
            }
            finally{
            	if (in != null) {
                    try {
                        in.close();
                    } catch (java.io.IOException e) {}
                }
            }
		}
	}
	
	void deleteFile(String path){
		File f = new File(path);

        if(f != null){
            f.delete();
        }
	}
	
	void loadUserInfo(){
		File f = mMainUI.getFilesDir();
		String root = f.getPath();
		
		File dir = new File(root + "/"+TALK_USER_INFO);
		
		if (dir.exists() == true) {
            java.io.FilenameFilter filter = new java.io.FilenameFilter() {
                public boolean accept(File dir, String name) {
                    return name.endsWith(".userinfo");
                }
            };

            File[] pageDirs = dir.listFiles(filter);

            for (int i = 0; pageDirs != null && i < pageDirs.length; i++) {
                UserInfo sc = new UserInfo();

                readUserInfo(pageDirs[i], sc);
                Log.d(TAG, "Read save info: name=" + sc.name + " ipAddr=" + sc.ipAddr);
                mListAdapter.handlePeerBroadcast(sc);
            }
        }
	}
	
	public void setSavedState(Object savedState){
		mLastStates = (SavedState)savedState;
		if(mLastStates != null){
			mListAdapter.mUserList = mLastStates.userList;
		}
	}
	
	public Object onRetainNonConfigurationInstance (){
		SavedState saved = new SavedState();

		saved.userList = mListAdapter.mUserList;
		return saved;	
	}
	
	private class MyCustomAdapter extends BaseAdapter {
		
		public List<UserInfo> mUserList = new ArrayList<UserInfo>();
		
		UserInfo findUserByIp(UserInfo user){
			Iterator<UserInfo> it = this.mUserList.iterator();
			while (it.hasNext()) {
				UserInfo u = (UserInfo) it.next();
				//Log.d(TAG, "user12="+u+" u.id="+u.sessionID);
				if(u.ipAddr.equals(user.ipAddr)){
					return u;
				}
			}
			return null;
		}
		
		UserInfo findUserBySessionID(int id){
			Iterator<UserInfo> it = this.mUserList.iterator();
			while (it.hasNext()) {
				UserInfo u = (UserInfo) it.next();
				Log.d(TAG, "user12="+u+" u.id="+u.sessionID);
				if(u.sessionID == id){
					return u;
				}
			}
			return null;
		}
		
		UserInfo findUserByIp(String ip){
			Iterator<UserInfo> it = this.mUserList.iterator();
			while (it.hasNext()) {
				UserInfo u = (UserInfo) it.next();
				//Log.d(TAG, "user12="+u+" u.id="+u.sessionID);
				if(u.ipAddr.equals(ip)){
					return u;
				}
			}
			return null;
		}
		
		void handlePeerBroadcast(UserInfo item){
			UserInfo existdUser = findUserByIp(item);
			if(existdUser == null){
				item.type = 1;
				mUserList.add(item);
				notifyDataSetChanged();
			}
			else if(!item.isSame(existdUser)){
				existdUser.name = item.name;
				existdUser.description = item.description;
				existdUser.beat = 10;
				notifyDataSetChanged();
			}
			else{
				existdUser.beat = 10;
				notifyDataSetChanged();
			}
		}
		
		void addUserForce(UserInfo user){
			UserInfo existdUser = findUserByIp(user);
			if(existdUser==null){
				user.type = 2;
				mUserList.add(user);
				notifyDataSetChanged();
			}
			else{
				existdUser.sessionID = user.sessionID;
			}
		}
		
		void handleTimeEvent(){
			boolean status_change = false;
			
			Iterator<UserInfo> it = this.mUserList.iterator();
			while (it.hasNext()) {
				UserInfo u = (UserInfo) it.next();
				if(u.type == 1){
					if(u.beat > 0){
						u.beat--;
					}
					else{
						status_change = true;
					}
				}
			}
			
			if(status_change){
				notifyDataSetChanged();
			}
		}
		@Override
		public int getCount() {
			return mUserList.size();
		}

		@Override
		public Object getItem(int arg0) {
			// TODO Auto-generated method stub
			return null;
		}

		@Override
		public long getItemId(int arg0) {
			// TODO Auto-generated method stub
			return 0;
		}

		@Override
		public View getView(int position, View convertView, ViewGroup parent)  {
			ViewHolder holder = null;
			if (convertView == null) {
                convertView = View.inflate(mMainUI, R.layout.list_items, null);
                holder = new ViewHolder();
                holder.peer_name_view = (TextView)convertView.findViewById(R.id.peer_name);
                holder.description_view = (TextView)convertView.findViewById(R.id.description);
                holder.status_icon = (ImageView)convertView.findViewById(R.id.status_icon);
                holder.user_icon = (ImageView)convertView.findViewById(R.id.user_icon);
                convertView.setTag(holder);
            } else {
                holder = (ViewHolder)convertView.getTag();
            }
            holder.peer_name_view.setText(mUserList.get(position).name);
            if(mUserList.get(position).beat <= 0){
            	holder.status_icon.setImageResource(R.drawable.im_user_offline);
            	holder.description_view.setText(R.string.offline);
            }
            else{
            	holder.status_icon.setImageResource(R.drawable.im_user);
            	holder.description_view.setText(R.string.online);
            }
            return convertView;
		}
		
		private class ViewHolder{
			public TextView peer_name_view;
			public TextView description_view;
			public ImageView status_icon;
			public ImageView user_icon;
		}
		
	}
	
	public void showConfigureWindow(){

		int[] videoSizeArray = null;

		List<Size> videoSize = mMainUI.getSupportedVideoSizes();
		if(videoSize != null && videoSize.size() > 0){
			int availableSize = 0;
			for(int i=0; i<videoSize.size(); i++){
				if(videoSize.get(i).width < MAX_VIDEO_WIDTH && videoSize.get(i).height < MAX_VIDEO_HEIGHT){
					availableSize++;
				}
			}

			videoSizeArray = new int[availableSize*2];
			int j = 0;
			for(int i=0; i<videoSize.size(); i++){
				if(videoSize.get(i).width < MAX_VIDEO_WIDTH && videoSize.get(i).height < MAX_VIDEO_HEIGHT){
					videoSizeArray[j++] = videoSize.get(i).width;
					videoSizeArray[j++] = videoSize.get(i).height;
				}
			}
		}

		Intent intent = new Intent(mMainUI, ConfigActivity.class);
		intent.putExtra("videoSize", videoSizeArray);
		mMainUI.startActivityForResult(intent, MainUI.CONFIG_ACTIVITY);
	}

	private OnClickListener mOnConfigBtnClick = new OnClickListener(){

		public void onClick(View arg0) {
			showConfigureWindow();
		}
	};

	void handlePeerBroadcast(Object msg){
		mListAdapter.handlePeerBroadcast((UserInfo)msg);
	}

	UserInfo findUserBySessionID(int id){
		return mListAdapter.findUserBySessionID(id);
	}
	
	UserInfo findUserByIp(String ip){
		return mListAdapter.findUserByIp(ip);
	}
	
	void addUserForce(UserInfo u){
		mListAdapter.addUserForce(u);
	}
	void refreshList(){
		mListAdapter.notifyDataSetChanged();
	}
	
	void handleTimeEvent(){
		mListAdapter.handleTimeEvent();
	}
	
	public void destory(){

	}

	@Override
	public void onVisible(boolean yes) {
		Log.d(TAG, "onVisible="+yes);
		if(yes){
			loadUserInfo();
		}
	}

	/**
	 * Returns a IPAddressKeyListener that accepts the digits 0 through 9, plus the dot
	 * character, subject to IP address rules: the first character has to be a digit, and
	 * no more than 3 dots are allowed.
	 */
	public class IPAddressKeyListener extends NumberKeyListener {

		private char[] mAccepted;

		@Override
		protected char[] getAcceptedChars() {
			return mAccepted;
		}

		/**
		 * The characters that are used.
		 *
		 * @see KeyEvent#getMatch
		 * @see #getAcceptedChars
		 */
		private final char[] CHARACTERS = new char[] { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '.' };

		private IPAddressKeyListener() {
			mAccepted = CHARACTERS;
		}

		/**
		 * Display a number-only soft keyboard.
		 */
		public int getInputType() {
			return InputType.TYPE_CLASS_NUMBER | InputType.TYPE_NUMBER_FLAG_DECIMAL;
		}

		/**
		 * Filter out unacceptable dot characters.
		 */
		public CharSequence filter(CharSequence source, int start, int end, Spanned dest, int dstart, int dend) {
			Log.d(TAG, "Set connect button false");
			mConnectBtn.setEnabled(false);
			
			if (end > start) {
				String destTxt = dest.toString();
				String resultingTxt = destTxt.substring(0, dstart) + source.subSequence(start, end) + destTxt.substring(dend);
				
				
				if (!resultingTxt.matches ("^\\d{1,3}(\\.(\\d{1,3}(\\.(\\d{1,3}(\\.(\\d{1,3})?)?)?)?)?)?")) {
					return "";
				} else {
					String[] splits = resultingTxt.split("\\.");

					for (int i=0; i<splits.length; i++) {
						if (Integer.valueOf(splits[i]) > 255) {
							return "";
						}
					}
					
					if(splits.length == 4){
						Log.d(TAG, "Set connect button true");
						mConnectBtn.setEnabled(true);;
					}
				}
			}
			return null;
		}
	}
}
