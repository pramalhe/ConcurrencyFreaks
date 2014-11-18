package com.concurrencyfreaks.readindicator;

import java.util.concurrent.atomic.AtomicLong;

public class RIIngressEgressSingleAtomic implements ReadIndicator {
    private final AtomicLong ingress = new AtomicLong(0);
    private final AtomicLong egress = new AtomicLong(0);
    
    public void arrive() {
        ingress.getAndIncrement();
    }
    
    public void depart() {
        egress.getAndIncrement();
    }
    
    public boolean isEmpty() {
        // Order is _very_ important here
        return egress.get() == ingress.get();
    }
}
