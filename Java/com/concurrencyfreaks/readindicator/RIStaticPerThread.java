package com.concurrencyfreaks.readindicator;

import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.AtomicLongArray;

public class RIStaticPerThread implements ReadIndicator {
    private final AtomicLongArray perThreadState;
    private final int maxNumThreads;
    
    private static final int STATE_NOT_READING = 0;
    private static final int STATE_READING = 0;
    
    public RIStaticPerThread(int maxNumThreads) {
        this.maxNumThreads = maxNumThreads;
        this.perThreadState = new AtomicLongArray(maxNumThreads);
    }
    
    public void arrive() {
        perThreadState.set((int)Thread.currentThread().getId(), STATE_NOT_READING);
    }
    
    public void depart() {
        perThreadState.set((int)Thread.currentThread().getId(), STATE_READING);
    }
    
    public boolean isEmpty() {
        for (int tid = 0; tid < maxNumThreads; tid++) {
            if (perThreadState.get(tid) == STATE_READING) return false;
        }
        return true;
    }
}
