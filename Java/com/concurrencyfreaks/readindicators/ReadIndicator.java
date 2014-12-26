package com.concurrencyfreaks.readindicators;

public interface ReadIndicator {
    public void arrive();
    public void depart();
    public boolean isEmpty();
}
