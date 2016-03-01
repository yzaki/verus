package com.example.sam33.myapplication;

/**
 * Created by sam33 on 2016/2/23.
 */
import java.util.concurrent.Semaphore;



//  bring pthread mutex function to android
class lockSendingList {

    private static class SingletonHolder {
        public static final lockSendingList INSTANCE = new lockSendingList();
    }

    /**
     * Use this method to get a reference to the singleton instance of
     * {@link lockSendingList}
     *
     * @return the singleton instance
     */
    public static lockSendingList getInstance() {
        return SingletonHolder.INSTANCE;
    }

    /**
     * In this sample, we allow only one thread at at time to update the cache
     * in order to maintain consistency
     */
    private Semaphore writeLock = new Semaphore(1);

    /**
     * We allow 10 concurrent threads to access the cache at any given time
     */
    private Semaphore readLock = new Semaphore(10);

    public void getWriteLock() throws InterruptedException {
        writeLock.acquire();
    }

    public void releaseWriteLock() {
        writeLock.release();
    }

    public void getReadLock() throws InterruptedException {
        readLock.acquire();
    }

    public void releaseReadLock() {
        readLock.release();
    }
}

// oop style code to send log from verus core to userinterface

abstract class Logger{
    public static final int SYS_CORRECTSTART = 1;
    public static final int SYS_CORRECTFINISH = 2;
    public static final int SYS_ERROROCCURE = 3;
    abstract void stderr(String tag,String msg);
    abstract void stdout(String msg);
    abstract void logfile(String msg);
    abstract void system(int SystemMsg);
}