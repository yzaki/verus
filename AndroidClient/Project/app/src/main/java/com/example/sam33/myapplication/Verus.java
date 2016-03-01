package com.example.sam33.myapplication;

import android.util.Log;

import java.io.BufferedOutputStream;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;
import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.OutputStream;
import java.net.DatagramPacket;
import java.net.DatagramSocket;
import java.net.InetAddress;
import java.net.Socket;
import java.net.SocketException;
import java.net.SocketTimeoutException;
import java.net.UnknownHostException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Date;
import java.util.List;
import java.util.Timer;
import java.util.TimerTask;

/**
 * Created by sam33 on 2016/2/15.
 *
 * This file is verus client code in java with Android SDK
 *
 */

public class Verus {


    class Timestamp
    {
        public int year;
        public int month;
        public int date;
        public int hour;
        public int minute;
        public int second;
        public int micro;
        public Timestamp()
        {
            Calendar Cld = Calendar.getInstance();
            year = Cld.get(Calendar.YEAR) ;
            month = Cld.get(Calendar.MONTH)+1;
            date = Cld.get(Calendar.DATE);
            hour = Cld.get(Calendar.HOUR_OF_DAY);
            minute = Cld.get(Calendar.MINUTE);
            second = Cld.get(Calendar.SECOND);
            micro = 1000*Cld.get(Calendar.MILLISECOND);
        }
    }


    class SendingThread extends Thread
    {
        public void run()
        {
            try {

                int z;
                Date timestamp;
                sendPkt pkt = null;

                try {
                    s1 = new DatagramSocket(Integer.parseInt(Port));
                } catch (SocketException e) {
                    throw new VerusExecption("socket() {SendingThread} port="+Port+" Srve_addr="+Srvr_addr.getHostAddress());
                }
                while (!terminate) {
                    Timestamp ts = new Timestamp();
                    lockSendingList.getInstance().getWriteLock();
                    if (sendingList.size() > 0)
                        pkt = sendingList.get(0);
                    // since tc qdisc command in Linux seems to have some issues when adding delay, we defer the packet here
                    if( sendingList.size() > 0 && ((ts.second - pkt.seconds)*1000+(ts.micro - pkt.micro)/1000 > delayms))
                    {
                        // sending ACK
                        byte buffer[] = "Hallo".getBytes();
                        DatagramPacket packet = new DatagramPacket(pkt.toByteArray(), pkt.toByteArray().length, Srvr2_addr, Integer.parseInt(Port));
                        try {
                            s.send(packet);
                        } catch (IOException ex) {
                            throw new VerusExecption("sendto(2)");
                        }
                        catch(Exception ex2) {
                            throw new VerusExecption("reached maximum OS UDP buffer size");
                        }
                        sendingList.remove(0);
                        lockSendingList.getInstance().releaseWriteLock();
                    }
                    else
                    {
                        lockSendingList.getInstance().releaseWriteLock();
                        //usleep(0.01); Android SDK 做不到 0.01的 usleep
                    }
                }
                s1.close();;
            }catch(Exception ex)
            {
                Error("SendingThread", ex.toString());
            }
        }
    }

    class TimeoutThread extends TimerTask
    {
        public void run()
        {
            try {
                sendPkt pkt = null;
                byte buffer[] = "Hallo".getBytes();
                DatagramPacket packet = new DatagramPacket(buffer,buffer.length, Srvr2_addr, Integer.parseInt(Port));
                try {
                    s.send(packet);
                }catch (IOException ex) {
                    throw new VerusExecption("sendto(Hallo)");
                }
            }catch(Exception ex)
            {
                Error("TimeoutThread", ex.toString());
            }
        }
    }

    public static final int SS_INIT_TIMEOUT = 1000;
    InetAddress Srvr_addr = null;
    InetAddress Srvr2_addr = null;
    String Port;
    DatagramSocket s;
    DatagramSocket s1;
    Timer timeout_tid;
    Thread sending_tid;
    boolean terminate = false;
    boolean receivedPkt = false;
    List<sendPkt> sendingList;
    long delayms = 0;
    Logger output = null;

    public void setOutput(Logger output)
    {
        this.output = output;
    }

    int Delay = 0;

    public Verus()
    {
        sendingList = new ArrayList<sendPkt>();
    }

    class VerusExecption extends Exception
    {
        String VerusLog;
        public VerusExecption(String ExecptionLog)
        {
            VerusLog = ExecptionLog;
        }
        @Override
        public String toString()
        {
            return super.toString()+"\n"+VerusLog;
        }
    }


    public void VerusLog(String str)
    {
        if(output!=null) {
            Log.e("Log",str);
            output.logfile(str);
        }
    }

    public void Log(String str)
    {
        if(output!=null) {
            output.stdout(str);
        }
    }

    public void Error(String tag,String str)
    {
        if(output!=null) {

            try {
                if (s != null)
                    s.close();
                if (s1 != null)
                    s1.close();
            }catch (Exception e) {

            }
            output.stderr(tag, str);
            output.system(Logger.SYS_ERROROCCURE);
        }
    }

    public void Start(String args[]) {
        try {
            String data = "Start Verus for android with follow options : \n";
            for(int i=0 ; i<args.length ; i++)
                data+=(args[i]+" ");
            data+="\n";
            Log(data);
            int z;
            int i = 1;
            String command;
            String tmp;
            udp_packet_t pdu;
            sendPkt pkt;
            if (args.length < 4) {
                throw new VerusExecption("syntax should be ./verus_client <server address> -p <server port> [-d <additional link delay in ms>] \n");
            }

            try {
                Srvr_addr = InetAddress.getByName(args[1]);
                Srvr2_addr = InetAddress.getByName(args[1]);
            } catch (UnknownHostException e) {
                throw new VerusExecption("bad address.\n");
            }
            while (i != (args.length - 1)) { // Check that we haven't finished parsing already
                i = i + 1;
                if(args[i].equals("-p")) {
                    i = i + 1;
                    Port = args[i];
                } else if (args[i].equals("-d")) {
                    i = i + 1;
                    Delay = Integer.parseInt(args[i]);
                } else {
                    throw new VerusExecption("syntax should be ./verus_client <server address> -p <server port> [-d <additional link delay in ms>]");
                }
            }
            try {
                s = new DatagramSocket(Integer.parseInt(Port));
            } catch (SocketException e) {
                throw new VerusExecption("socket() {#Start} port="+Port+" Srve_addr="+Srvr_addr.getHostAddress()+">>\n"+e.toString());
            }
            Log("Sending request to server \n");
            if(output!=null)
                output.system(Logger.SYS_CORRECTSTART);
            byte buffer[] = "Hallo".getBytes();
            DatagramPacket packet = new DatagramPacket(buffer,buffer.length, Srvr_addr, Integer.parseInt(Port));
            try {
                s.send(packet);
            }catch (IOException ex) {
                throw new VerusExecption("sendto(Hallo)");
            }
            try{
                sending_tid = new Thread();
                sending_tid.start();
            }catch (Exception threadex)
            {
                throw new VerusExecption("can't create thread");
            }

            timeout_tid = new Timer(true);
            timeout_tid.schedule(new TimeoutThread(),0,SS_INIT_TIMEOUT);

            // starting to loop waiting to receive data and to ACK
            while (!terminate) {
                byte rcvbuf[] = new byte[4096];
                DatagramPacket rcvpacket = new DatagramPacket(rcvbuf,rcvbuf.length);
                try {
                    s.setSoTimeout(3000);
                    s.receive(rcvpacket);
                } catch (SocketTimeoutException tx){
                    throw new VerusExecption("recvfrom(2) timeout after send Hallo");
                } catch (IOException e) {
                    throw new VerusExecption("recvfrom(2)");
                }
                pdu = new udp_packet_t();
                try {
                    pdu.fromByteArray(rcvbuf);
                } catch (IOException e) {
                    throw new VerusExecption("pdu.fromByteArray");

                }
                if(pdu.ss_id < 0)
                {
                    terminate = true;
                }
                // stopping the io timer for the timeout
                if (!receivedPkt) {
                    receivedPkt = true;
                    Log("Connected to server");
                }
                Timestamp ts = new Timestamp();
                VerusLog(ts.second+"."+ts.micro+", "+pdu.seq);
                pkt = new sendPkt();
                pkt.pdu = pdu;
                pkt.seconds = ts.second;
                pkt.micro = ts.micro;

                try {
                    lockSendingList.getInstance().getWriteLock();
                } catch (InterruptedException e) {
                    throw new VerusExecption("collision at lockSendingList");
                }
                sendingList.add(pkt);
                lockSendingList.getInstance().releaseWriteLock();

            }
            Log("Client exiting\n");
            output.system(Logger.SYS_CORRECTFINISH);
            s.close();
        }catch (VerusExecption verusex)
        {
            Error("VerusMain", verusex.toString());
        }
    }

    class udp_packet_t{
        int ss_id;
        long seq;
        long w;
        long seconds;
        long micro;
        public byte[] toByteArray() throws IOException {
            ByteArrayOutputStream bs = new ByteArrayOutputStream();
            DataOutputStream ds = new DataOutputStream(bs);
            ds.writeInt(ss_id);
            ds.writeLong(seq);
            ds.writeLong(w);
            ds.writeLong(seconds);
            ds.writeLong(micro);
            return bs.toByteArray();
        }
        public void fromByteArray(byte buf[]) throws IOException {
            ByteArrayInputStream bs = new ByteArrayInputStream(buf);
            DataInputStream ds = new DataInputStream(bs);
            ss_id = ds.readInt();
            seq = ds.readLong();
            w = ds.readLong();
            seconds = ds.readLong();
            micro = ds.readLong();
        }

    }

    class sendPkt {
        udp_packet_t pdu;
        long seconds;
        long micro;
        public byte[] toByteArray() throws IOException {
            ByteArrayOutputStream bs = new ByteArrayOutputStream();
            DataOutputStream ds = new DataOutputStream(bs);
            ds.write(pdu.toByteArray());
            ds.writeLong(seconds);
            ds.writeLong(micro);
            return bs.toByteArray();
        }
    }

}
