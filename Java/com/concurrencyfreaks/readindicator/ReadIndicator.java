package com.concurrencyfreaks.readindicator;

public interface ReadIndicator {

    public void arrive();
    public void depart();
    public boolean isEmpty();
}
