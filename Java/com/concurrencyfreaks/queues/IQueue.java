// This is used to simplify the code for the Stress tests and performance tests
package com.concurrencyfreaks.queues;

public interface IQueue<E> {
    public void enqueue(E item);
    public E dequeue();
}
