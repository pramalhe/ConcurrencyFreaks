package com.concurrencyfreaks.readindicator;

import java.util.concurrent.atomic.AtomicLong;

public class RIAtomicCounter implements ReadIndicator {
    private final AtomicLong counter = new AtomicLong(0);
    
    public void arrive() {
        counter.getAndIncrement();
    }
    
    public void depart() {
        counter.getAndDecrement();
    }
    
    public boolean isEmpty() {
        return counter.get() > 0;
    }
    
}
