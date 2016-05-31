package com.example.sam33.myapplication;

import android.os.Environment;
import android.os.Handler;
import android.os.HandlerThread;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;
import android.view.Menu;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;
import android.widget.EditText;
import android.widget.ScrollView;
import android.widget.Switch;
import android.widget.TextView;

import java.io.DataOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.StringTokenizer;

/**
 * Created by sam33 on 2016/2/23.
 *
 * This file is Android activity user interface to run Verus.
 *
 */


public class MainActivity extends AppCompatActivity{

    Button StartButton;
    TextView LogView;
    EditText CmdLine;
    ScrollView Scroll;
    Handler BackgroundHandler;
    Handler UIHandler;
    HandlerThread BackgroundThread;
    Verus verus;



    //User define logger to process Logs from Verus core
    class LoggerCTL extends Logger
    {
        String clientLogString="";
        DataOutputStream clientLog = null;
        String location;

        public LoggerCTL()
        {
            try {
                File sdCard = Environment.getExternalStorageDirectory();
                File dir = new File(sdCard.getAbsolutePath()+"/verus");
                location = sdCard.getAbsolutePath()+"/verus"+"/clientLog.txt";
                dir.mkdirs();;
                File file1 = new File(dir, "clientLog.txt");
                clientLog = new DataOutputStream(new FileOutputStream(file1));
            } catch (FileNotFoundException ex) {
                UIHandler.post(new UIOutput("can't opem "+location,UIOutput.CLEARALL));
                StartButton.setEnabled(true);
            }
        }

        void writeToFile()
        {
            char buf1[] = clientLogString.toCharArray();
            for(int i=0 ; i<buf1.length ; i++)
            {
                try {
                    clientLog.writeChar(buf1[i]);
                } catch (IOException e) {
                    UIHandler.post(new UIOutput("can't write clientLog.txt : \n"+e.toString(),UIOutput.CLEARALL));
                    break;
                }
            }
            try {
                clientLog.close();
            } catch (IOException e) {
                UIHandler.post(new UIOutput("can't close clientLog.txt",UIOutput.CLEARALL));
            }
        }
        @Override
        void stderr(String tag, String msg) {
            UIHandler.post(new UIOutput(msg,UIOutput.CLEARALL));
        }

        @Override
        void stdout(String msg) {
            UIHandler.post(new UIOutput(msg,UIOutput.ADDTAIL));
        }

        @Override
        void logfile(String msg) {
            UIHandler.post(new UIOutput(msg,UIOutput.ADDTAIL));
            clientLogString+=msg;
        }

        @Override
        void system(int SystemMsg) {
            switch (SystemMsg)
            {
                case Logger.SYS_CORRECTFINISH:
                    UIHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            writeToFile();
                            UIHandler.post(new UIOutput("Save logfile to "+location,UIOutput.ADDTAIL));
                            verus.setOutput(null);
                            StartButton.setEnabled(true);
                        }
                    });
                    break;

                case Logger.SYS_CORRECTSTART:
                    UIHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            StartButton.setEnabled(false);
                        }
                    });
                    break;
                case Logger.SYS_ERROROCCURE:
                    UIHandler.post(new Runnable() {
                        @Override
                        public void run() {
                            writeToFile();
                            verus.setOutput(null);
                            StartButton.setEnabled(true);
                        }
                    });
                    break;
            }
        }
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        StartButton = (Button)findViewById(R.id.button);
        Scroll = (ScrollView)findViewById(R.id.textscroll);
        LogView = (TextView)findViewById(R.id.Log);
        CmdLine = (EditText)findViewById(R.id.CmdLine);
        BackgroundThread = new HandlerThread("Background");
        BackgroundThread.start();
        UIHandler = new Handler();
        BackgroundHandler = new Handler(BackgroundThread.getLooper());
        StartButton.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                 BackgroundHandler.post(new StartVerus(CmdLine.getText().toString()));
            }
        });
    }

    @Override
    public boolean onCreateOptionsMenu(Menu menu) {
        // Inflate the menu; this adds items to the action bar if it is present.
        getMenuInflater().inflate(R.menu.menu_main, menu);
        return true;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        // Handle action bar item clicks here. The action bar will
        // automatically handle clicks on the Home/Up button, so long
        // as you specify a parent activity in AndroidManifest.xml.
        int id = item.getItemId();
        //noinspection SimplifiableIfStatement
        if (id == R.id.action_settings) {
            return true;
        }
        return super.onOptionsItemSelected(item);
    }

    //Output Runnable to send message from background thread to UI thread.
    class UIOutput implements Runnable
    {
        public static final int ADDTAIL = 1;
        public static final int CLEARALL = 2;
        int Option;
        String msg;
        public UIOutput(String msg,int Option)
        {
            this.Option = Option;
            this.msg = msg;
        }
        @Override
        public void run() {
            if(Option==ADDTAIL)
                LogView.setText(LogView.getText()+msg+"\n");
            if(Option==CLEARALL) {
                LogView.setText(msg+"\n");
            }
            Scroll.fullScroll(ScrollView.FOCUS_DOWN);
        }
    }

    //Start verus from background thread
    class StartVerus implements Runnable
    {
        String cmd;
        public StartVerus(String cmd)
        {
            this.cmd = cmd;
        }

        @Override
        public void run() {
            StringTokenizer st = new StringTokenizer(cmd," ");
            String args[] = new String[st.countTokens()];
            int size = st.countTokens();
            for(int i=0 ; i<size ; i++) {
                args[i] = st.nextToken();
            }
            verus = new Verus();
            LoggerCTL output = new LoggerCTL();
            verus.setOutput(output);
            verus.Start(args);
        }
    }
}