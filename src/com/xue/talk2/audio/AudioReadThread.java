package com.xue.talk2.audio;

import android.media.AudioFormat;
import android.media.AudioRecord;
import android.media.MediaRecorder;

public class AudioReadThread extends Thread{

	static final int frequency = 44100;   
	static final int channelConfiguration = AudioFormat.CHANNEL_CONFIGURATION_MONO;   
	static final int audioEncoding = AudioFormat.ENCODING_PCM_16BIT;
	
	private int recBufSize;
	private AudioRecord audioRecord;
	private boolean isRecording;
	
	public AudioReadThread(){
		recBufSize = AudioRecord.getMinBufferSize(frequency,   
				channelConfiguration, audioEncoding);     

		audioRecord = new AudioRecord(MediaRecorder.AudioSource.MIC, frequency,   
				channelConfiguration, audioEncoding, recBufSize);
		
		isRecording = true;
	}
	
	public void run(){
		byte[] buffer = new byte[recBufSize];   
        audioRecord.startRecording();//开始录制   
            
        while (isRecording) {   
            //从MIC保存数据到缓冲区   
            int bufferReadResult = audioRecord.read(buffer, 0,   
                    recBufSize);   

            byte[] tmpBuf = new byte[bufferReadResult];   
            System.arraycopy(buffer, 0, tmpBuf, 0, bufferReadResult);   
            //写入数据即播放   
            //audioTrack.write(tmpBuf, 0, tmpBuf.length);   
        }   
       // audioTrack.stop();   
        audioRecord.stop();   
	}
}
