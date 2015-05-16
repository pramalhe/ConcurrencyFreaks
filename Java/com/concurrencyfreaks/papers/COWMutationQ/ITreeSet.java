package com.concurrencyfreaks.papers.COWMutationQ;

import java.util.Collection;

public interface ITreeSet<E> {
    public boolean contains(Object key);
    public boolean add(E key);
    public boolean remove(E key);
    public boolean addAll(Collection<? extends E> c);
}
