/******************************************************************************
 * Copyright (c) 2015, Pedro Ramalhete, Andreia Correia
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of Concurrency Freaks nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL <COPYRIGHT HOLDER> BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 ******************************************************************************
 */
package com.concurrencyfreaks.papers.COWMutationQ;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.concurrent.ConcurrentLinkedQueue;
import com.concurrencyfreaks.tests.CSVDatabase;



/**
 * This is a micro-benchmark to run the tests shown in COW+MutationQ paper
 * 
 * It has only Mixed mode:
 * Uses write-per-mil to decide if a randomly chosen element is going to
 * be added()/removed() or contains().
 * 
 */
public class BenchmarkTreeSets {
    
    public enum TestCase {
        ReentrantLockTreeSet,        // Mutex (ReentrantLock)
        StampedLockTreeSet,          // StampedLock (RW-Lock)
        CRWWPLockTreeSet,            // Scalable RWLock with LongAdder ingress/egress
        LeftRightTreeSet,            // Left-Right with CLQ+Array+Finalizers ReadIndicator
        CopyOnWriteLockTreeSet,      // COW + Lock
        CopyOnWriteCASTreeSet,       // COW + CAS
        CopyOnWriteMQLFTreeSet,      // COW + Mutation Q (Lock-Free)
        CopyOnWriteMQWFTreeSet,      // COW + Mutation Q (Wait-Free)
        MAX_TEST_CASES,
    }
    
    private final static int numElements = 100;            // Number of elements in the array: 1000, 10k, 100k, 1M
    private final static int MAX_LINEAR_HIST_BINS = 1000;      // 1000 when in doubt, but 200 is usually enough
    private final static int LINEAR_BIN_WIDTH = 1000;          // width of bins is 1000 nanosecond (1 micro-second)

    private final int numMilis;  
    private final WorkerThread[] workerThreads;
    private final LatencyThread[] latencyThreads;
    private final UserData[] udArray;
    private final CSVDatabase csvdb;
    
    private final ConcurrentLinkedQueue<UserData> clq;    // We use this only to make faster addAll()
    private final ReentrantLockTreeSet<UserData> mutexTreeSet;
    private final StampedLockTreeSet<UserData> stampedTreeSet;
    private final CRWWPLockTreeSet<UserData> crwwpTreeSet;
    private final LeftRightTreeSet<UserData> leftRightTreeSet;
    private final CopyOnWriteLockTreeSet<UserData> cowlockTreeSet;
    private final CopyOnWriteCASTreeSet<UserData> casTreeSet;
    private final CopyOnWriteMQLFTreeSet<UserData> mqlfTreeSet;
    private final CopyOnWriteMQWFTreeSet<UserData> mqwfTreeSet;
    
    private int writePerMil = -1; 
    
    public BenchmarkTreeSets(int numThreads, int numMilis, CSVDatabase csvdb, int writePerMil) {
        clq = new ConcurrentLinkedQueue<UserData>();
        mutexTreeSet = new ReentrantLockTreeSet<UserData>();
        stampedTreeSet = new StampedLockTreeSet<UserData>();
        crwwpTreeSet = new CRWWPLockTreeSet<UserData>();
        leftRightTreeSet = new LeftRightTreeSet<UserData>();
        cowlockTreeSet = new CopyOnWriteLockTreeSet<UserData>();
        casTreeSet = new CopyOnWriteCASTreeSet<UserData>();
        mqlfTreeSet = new CopyOnWriteMQLFTreeSet<UserData>();
        mqwfTreeSet = new CopyOnWriteMQWFTreeSet<UserData>();
        
        this.numMilis = numMilis;
        this.csvdb = csvdb;
        this.writePerMil = writePerMil;
        udArray = new UserData[numElements];
        for (int i = 0; i < numElements; i++) {
            udArray[i] = new UserData();
            udArray[i].a = i;
            udArray[i].b = i % 7;
        }
                
        System.out.println("Filling up data structures");
        for (int i = 0; i < numElements; i++) clq.add(udArray[i]);
        mutexTreeSet.addAll(clq);
        stampedTreeSet.addAll(clq);
        crwwpTreeSet.addAll(clq);
        leftRightTreeSet.addAll(clq);
        cowlockTreeSet.addAll(clq);
        casTreeSet.addAll(clq);
        mqlfTreeSet.addAll(clq);
        mqwfTreeSet.addAll(clq);

        workerThreads = new WorkerThread[numThreads];
        latencyThreads = new LatencyThread[numThreads];
    }
    
    /**
     * By having the constructor and the tests separated, we can introduce a 
     * delay that will increase the chances that the GC will go in.
     * @param numThreads
     */
    public void performanceTests(int numThreads, int numRuns) {
        /* Performance tests */
        double writePercentage = writePerMil == 0 ? 0 : writePerMil/10.;
        System.out.println("----- numThreads=" +numThreads+"  Writes="+writePercentage+"%  numElements="+numElements+"  numRuns="+numRuns+" -----");        
        //singleThroughputTest(numThreads, TestCase.ReentrantLockTreeSet, numRuns);
        //singleThroughputTest(numThreads, TestCase.StampedLockTreeSet, numRuns);
        //singleThroughputTest(numThreads, TestCase.CRWWPLockTreeSet, numRuns);
        //singleThroughputTest(numThreads, TestCase.LeftRightTreeSet, numRuns);
        singleThroughputTest(numThreads, TestCase.CopyOnWriteLockTreeSet, numRuns);
        singleThroughputTest(numThreads, TestCase.CopyOnWriteCASTreeSet, numRuns);
        singleThroughputTest(numThreads, TestCase.CopyOnWriteMQLFTreeSet, numRuns);
        singleThroughputTest(numThreads, TestCase.CopyOnWriteMQWFTreeSet, numRuns);
        System.out.println(); 
    }

    
    /**
     * Main performance benchmark
     */
    public void singleThroughputTest(int numThreads, TestCase type, int numRuns) {
        // If we see an error here just increase the number of spaces
        String indentedName = type.toString() + "                           ".substring(type.toString().length());
        System.out.print("##### "+indentedName+" #####  ");
        long[] numAddRemoves = new long[numRuns];
        long[] numContains = new long[numRuns];

        for (int irun = 0; irun < numRuns; irun++) {
            // Create the threads and then start them all in one go
            for (int i = 0; i < numThreads; i++) {
                workerThreads[i] = new WorkerThread(type, i);
            }
            for (int i = 0; i < numThreads; i++) workerThreads[i].start();
    
            // Sleep for 10 seconds
            try {
                Thread.sleep(numMilis);
            } catch(InterruptedException e) {
                System.out.println("InterruptedException");
            }
            for (int i = 0; i < numThreads; i++) workerThreads[i].quit = true;
                   
            try {
                for (int i = 0; i < numThreads; i++) workerThreads[i].join();
            } catch (InterruptedException e) {
                e.printStackTrace();
            }
            
            numAddRemoves[irun] = 0;
            numContains[irun] = 0;
            for (int i = 0; i < numThreads; i++) {
                numAddRemoves[irun] += workerThreads[i].numAddRemoves;
                numContains[irun] += workerThreads[i].numContains;
            }
        }
        
        // Now compute the median
        int medianRun = numRuns/2; // numRuns should be an odd number
        long medianAddRemoves = 0;
        long medianContains = 0;
        if (numContains[0] != 0) {
            long[] sortedCopy = Arrays.copyOf(numContains, numContains.length);
            Arrays.sort(sortedCopy);
            for (int irun = 0; irun < numRuns; irun++) {
                if (sortedCopy[medianRun] == numContains[irun]) {
                    medianAddRemoves = numAddRemoves[irun];
                    medianContains = numContains[irun];
                    break;
                }
            }
        } else {
            long[] sortedCopy = Arrays.copyOf(numAddRemoves, numAddRemoves.length);
            Arrays.sort(sortedCopy);
            for (int irun = 0; irun < numRuns; irun++) {
                if (sortedCopy[medianRun] == numAddRemoves[irun]) {
                    medianAddRemoves = numAddRemoves[irun];
                    medianContains = numContains[irun];
                    break;
                }
            }
        }
        final int perSecond = numMilis/1000;
        System.out.println("numAddRemoves/sec = "+(medianAddRemoves/perSecond)+"  numContains/sec = "+(medianContains/perSecond));

        if (writePerMil == -1) {
            csvdb.addRunDedicated(type.toString(), numThreads-2, 2, medianContains/perSecond, medianAddRemoves/perSecond);
        } else {
            csvdb.addRun(type.toString(), writePerMil, numThreads, (medianContains+medianAddRemoves)/perSecond);
        }
    }

    
    /**
     * Half the threads will be reading only, and half the threads will be writing only
     */
    public void singleLatencyTest(TestCase type) {
        
        // If we see an error here just increase the number of spaces
        String indentedName = type.toString() + "                           ".substring(type.toString().length());
        System.out.print("##### "+indentedName+" #####  ");

        // Create 16 threads and then start them all in one go
        int numThreads = 16;
        latencyThreads[0] = new LatencyThread(type, 0, true);
        latencyThreads[1] = new LatencyThread(type, 1, true);
        latencyThreads[2] = new LatencyThread(type, 2, true);
        latencyThreads[3] = new LatencyThread(type, 3, true);
        latencyThreads[4] = new LatencyThread(type, 4, true);
        latencyThreads[5] = new LatencyThread(type, 5, true);
        latencyThreads[6] = new LatencyThread(type, 6, true);
        latencyThreads[7] = new LatencyThread(type, 7, true);
        latencyThreads[8] = new LatencyThread(type, 8, false);
        latencyThreads[9] = new LatencyThread(type, 9, false);
        latencyThreads[10] = new LatencyThread(type, 10, false);
        latencyThreads[11] = new LatencyThread(type, 11, false);
        latencyThreads[12] = new LatencyThread(type, 12, false);
        latencyThreads[13] = new LatencyThread(type, 13, false);
        latencyThreads[14] = new LatencyThread(type, 14, false);
        latencyThreads[15] = new LatencyThread(type, 15, false);
        for (int i = 0; i < numThreads; i++) latencyThreads[i].start();

        // Sleep for 10 seconds
        try {
            Thread.sleep(numMilis);
        } catch(InterruptedException e) {
            System.out.println("InterruptedException");
        }
        for (int i = 0; i < numThreads; i++) latencyThreads[i].quit = true;
               
        try {
            for (int i = 0; i < numThreads; i++) latencyThreads[i].join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        
        // Aggregate the CONTAINS histograms from all the Reader threads
        final double[] containsLatencyLinearHistogram = new double[MAX_LINEAR_HIST_BINS];
        for (int i = 0; i < numThreads; i++) {
            for (int ibox = 0 ; ibox < containsLatencyLinearHistogram.length; ibox++) {
                containsLatencyLinearHistogram[ibox] += latencyThreads[i].containsLatencyLinearHistogram[ibox];
            }            
        }
        // Normalize the histograms
        double normFactor = 0;
        for (int ibox = 0 ; ibox < containsLatencyLinearHistogram.length; ibox++) {
            normFactor += containsLatencyLinearHistogram[ibox];
        }
        for (int ibox = 0 ; ibox < containsLatencyLinearHistogram.length; ibox++) {
            containsLatencyLinearHistogram[ibox] /= normFactor;
        }
        // Show the histograms in csv format
        System.out.println("--------------------- CONTAINS -----------------");
        for (int ibox = 0 ; ibox < containsLatencyLinearHistogram.length; ibox++) {
            System.out.println(ibox+", "+containsLatencyLinearHistogram[ibox]);
        }

        // Aggregate the ADD histograms from all the Writer threads
        final double[] addLatencyLinearHistogram = new double[MAX_LINEAR_HIST_BINS];
        for (int i = 0; i < numThreads; i++) {
            for (int ibox = 0 ; ibox < addLatencyLinearHistogram.length; ibox++) {
                addLatencyLinearHistogram[ibox] += latencyThreads[i].addLatencyLinearHistogram[ibox];
            }            
        }
        // Normalize the histograms
        normFactor = 0;
        for (int ibox = 0 ; ibox < addLatencyLinearHistogram.length; ibox++) {
            normFactor += addLatencyLinearHistogram[ibox];
        }
        for (int ibox = 0 ; ibox < addLatencyLinearHistogram.length; ibox++) {
            addLatencyLinearHistogram[ibox] /= normFactor;
        }
        // Show the histograms in csv format
        System.out.println("--------------------- ADD -----------------");
        for (int ibox = 0 ; ibox < addLatencyLinearHistogram.length; ibox++) {
            System.out.println(ibox+", "+addLatencyLinearHistogram[ibox]);
        }

        // Aggregate the REMOVE histograms from all the Writer threads
        final double[] removeLatencyLinearHistogram = new double[MAX_LINEAR_HIST_BINS];
        for (int i = 0; i < numThreads; i++) {
            for (int ibox = 0 ; ibox < removeLatencyLinearHistogram.length; ibox++) {
                removeLatencyLinearHistogram[ibox] += latencyThreads[i].removeLatencyLinearHistogram[ibox];
            }            
        }
        // Normalize the histograms
        normFactor = 0;
        for (int ibox = 0 ; ibox < removeLatencyLinearHistogram.length; ibox++) {
            normFactor += removeLatencyLinearHistogram[ibox];
        }
        for (int ibox = 0 ; ibox < removeLatencyLinearHistogram.length; ibox++) {
            removeLatencyLinearHistogram[ibox] /= normFactor;
        }
        // Show the histograms in csv format
        System.out.println("--------------------- REMOVE -----------------");
        for (int ibox = 0 ; ibox < removeLatencyLinearHistogram.length; ibox++) {
            System.out.println(ibox+", "+removeLatencyLinearHistogram[ibox]);
        }
        
        System.out.println();
    }
    
    
    /**
     * Inner class for user's data that will be used as "Value" of the HashMap
     */
    public class UserData implements Comparable<UserData>{
        public int a = 1;
        public int b = 2;
        public int compareTo(UserData o){
            return this.a-o.a;
        }
    }    
    

    /**
     * An imprecise but fast random number generator
     */
    public static long randomLong(long x) {
        x ^= (x << 21);
        x ^= (x >>> 35);
        x ^= (x << 4);
        return x;
    }
    
    
    /**
     * Inner class for the Worker thread that does performance tests
     */
    class WorkerThread extends Thread {
        TestCase type;
        final int tid;
        volatile boolean quit = false;
        long numAddRemoves = 0;
        long numContains = 0;
        
        public WorkerThread(TestCase type, int tid) {
            this.type = type;   
            this.tid = tid;
        }
                
                        
        public void run() {
            long xrand = System.nanoTime();
            long x;
            
            while (!quit) {
                xrand = randomLong(xrand);
                if (xrand < 0) x = -xrand;
                else x = xrand;
                
                UserData ud = udArray[(int)(x % numElements)];
                
                xrand = randomLong(xrand);
                if (xrand < 0) x = -xrand;
                else x = xrand;
                
                UserData ud1 = udArray[(int)(x % numElements)];
                if ((writePerMil==-1 && (tid == 0 || tid == 1)) || 
                     (writePerMil != 0 && (x%1000) < writePerMil)) {
                    // These two threads are doing remove/add
                    switch (type) {
                    case ReentrantLockTreeSet:
                        mutexTreeSet.remove(ud);
                        mutexTreeSet.add(ud);
                        break;
                    case StampedLockTreeSet:
                        stampedTreeSet.remove(ud);
                        stampedTreeSet.add(ud);
                        break;
                    case LeftRightTreeSet:
                        leftRightTreeSet.remove(ud);
                        leftRightTreeSet.add(ud);
                        break;
                    case CRWWPLockTreeSet:
                        crwwpTreeSet.remove(ud);
                        crwwpTreeSet.add(ud);
                        break;
                    case CopyOnWriteLockTreeSet:
                        cowlockTreeSet.remove(ud);
                        cowlockTreeSet.add(ud);
                        break;
                    case CopyOnWriteCASTreeSet:
                        casTreeSet.remove(ud);
                        casTreeSet.add(ud);
                        break;
                    case CopyOnWriteMQLFTreeSet:
                        mqlfTreeSet.remove(ud);
                        mqlfTreeSet.add(ud);
                        break;
                    case CopyOnWriteMQWFTreeSet:
                        mqwfTreeSet.remove(ud);
                        mqwfTreeSet.add(ud);
                        break;
                    }
                    numAddRemoves += 2;
                } else {                   
                    switch (type) {
                    case ReentrantLockTreeSet:
                        mutexTreeSet.contains(ud);
                        mutexTreeSet.contains(ud1);
                        break;
                    case StampedLockTreeSet:
                        stampedTreeSet.contains(ud);
                        stampedTreeSet.contains(ud1);
                        break;
                    case CRWWPLockTreeSet:
                        crwwpTreeSet.contains(ud);
                        crwwpTreeSet.contains(ud1);
                        break;
                    case LeftRightTreeSet:
                        leftRightTreeSet.contains(ud);
                        leftRightTreeSet.contains(ud1);
                        break;
                    case CopyOnWriteLockTreeSet:
                        cowlockTreeSet.contains(ud);
                        cowlockTreeSet.contains(ud1);
                        break;
                    case CopyOnWriteCASTreeSet:
                        casTreeSet.contains(ud);
                        casTreeSet.contains(ud1);
                        break;
                    case CopyOnWriteMQLFTreeSet:
                        mqlfTreeSet.contains(ud);
                        mqlfTreeSet.contains(ud1);
                        break;
                    case CopyOnWriteMQWFTreeSet:
                        mqwfTreeSet.contains(ud);
                        mqwfTreeSet.contains(ud1);
                        break;
                    }
                    numContains += 2;
                }
            }
        }
    }

    
    /**
     * Inner class for the Worker thread that does Latency tests
     */
    class LatencyThread extends Thread {
        TestCase type;
        final int tid;
        final boolean isReader;
        final ITreeSet<UserData> treeSet;
        volatile boolean quit = false;
        final long[] containsLatencyLinearHistogram = new long[MAX_LINEAR_HIST_BINS];
        final long[] addLatencyLinearHistogram = new long[MAX_LINEAR_HIST_BINS];
        final long[] removeLatencyLinearHistogram = new long[MAX_LINEAR_HIST_BINS];
        
        public LatencyThread(TestCase type, int tid, boolean isReader) {
            this.type = type;   
            this.tid = tid;
            this.isReader = isReader;
            switch (type) {
            case ReentrantLockTreeSet:
                this.treeSet = mutexTreeSet;
                break;
            case StampedLockTreeSet:
                this.treeSet = stampedTreeSet;
                break;
            case CRWWPLockTreeSet:
                this.treeSet = crwwpTreeSet;
                break;
            case LeftRightTreeSet:
                this.treeSet = leftRightTreeSet;
                break;
            case CopyOnWriteLockTreeSet:
                this.treeSet = cowlockTreeSet;
                break;
            case CopyOnWriteCASTreeSet:
                this.treeSet = casTreeSet;
                break;                
            case CopyOnWriteMQLFTreeSet:
                this.treeSet = mqlfTreeSet;
                break;                
            case CopyOnWriteMQWFTreeSet:
                this.treeSet = mqwfTreeSet;
                break;                
            default:
                this.treeSet = null;
                break;
            }            
            for (int i = 0; i < MAX_LINEAR_HIST_BINS; i++) {
                containsLatencyLinearHistogram[i] = 0;
                addLatencyLinearHistogram[i] = 0;
                removeLatencyLinearHistogram[i] = 0;
            }
        }
                        
        public void run() {
            long xrand = System.nanoTime();
            long x; 
            
            while (!quit) {
                xrand = randomLong(xrand);
                if (xrand < 0) x = -xrand;
                else x = xrand;
                UserData ud = udArray[(int)(x % numElements)];
                if (!isReader) {
                    long startBeats = 0, stopBeats = 0, numBeats;
                    startBeats = System.nanoTime();
                    treeSet.remove(ud);
                    stopBeats = System.nanoTime();
                    numBeats = stopBeats-startBeats;
                    storeRemoveLinearLatency(numBeats);

                    startBeats = System.nanoTime();
                    treeSet.add(ud);
                    stopBeats = System.nanoTime();
                    numBeats = stopBeats-startBeats;
                    storeAddLinearLatency(numBeats);
                } else {                   
                    long startBeats = System.nanoTime();
                    treeSet.contains(ud);
                    long stopBeats = System.nanoTime();
                    final long numbeats = stopBeats-startBeats;
                    storeContainsLinearLatency(numbeats);
                }
            }
        }
        
        private void storeContainsLinearLatency(long delta) {
            int ibox = (int)(delta/LINEAR_BIN_WIDTH);
            if (ibox > containsLatencyLinearHistogram.length-1) ibox = containsLatencyLinearHistogram.length-1;
            containsLatencyLinearHistogram[ibox]++;
        }

        private void storeAddLinearLatency(long delta) {
            int ibox = (int)(delta/LINEAR_BIN_WIDTH);
            if (ibox > addLatencyLinearHistogram.length-1) ibox = addLatencyLinearHistogram.length-1;
            addLatencyLinearHistogram[ibox]++;
        }

        private void storeRemoveLinearLatency(long delta) {
            int ibox = (int)(delta/LINEAR_BIN_WIDTH);
            if (ibox > removeLatencyLinearHistogram.length-1) ibox = removeLatencyLinearHistogram.length-1;
            removeLatencyLinearHistogram[ibox]++;
        }
    }

    
    
    /**
     * Mixed task benchmark, where each thread may do either Writes or Reads
     * @throws InterruptedException 
     */
    static void throughputBenchmark() throws InterruptedException {
        //final LinkedList<Integer> threadList = new LinkedList(Arrays.asList(1, 2, 3, 4, 5, 6, 7, 8, 12, 16, 20, 24, 28, 32, 36, 40, 64));
        //final int numRuns = 5;
        //final int durationSeconds = 20;
        final LinkedList<Integer> threadList = new LinkedList(Arrays.asList(4));
        final int numRuns = 1;  // On the paper it should be 5 runs
        final int durationSeconds = 10;      // 20 seconds is the default for the paper
        
        final CSVDatabase csvdb = new CSVDatabase();      
        final int sleepGCMiliSeconds = 1000; // Number of miliseconds to sleep between tests for the GC to enter 
        BenchmarkTreeSets tests;
        System.out.println("This system has " + Runtime.getRuntime().availableProcessors() + " cores");

        for (Integer nThreads : threadList) {
            tests = new BenchmarkTreeSets(nThreads, durationSeconds*1000, csvdb, 1000);
            System.gc();
            Thread.sleep(sleepGCMiliSeconds); // sleep for a while second to allow the GC to work a bit                
            tests.performanceTests(nThreads, numRuns);
        }  // 50-50-0:  50% add() - 50% remove() - 0% contains() 

        for (Integer nThreads : threadList) {
            tests = new BenchmarkTreeSets(nThreads, durationSeconds*1000, csvdb, 500);
            System.gc();
            Thread.sleep(sleepGCMiliSeconds); // sleep for a while second to allow the GC to work a bit                
            tests.performanceTests(nThreads, numRuns);
        }  // 50%
        
        for (Integer nThreads : threadList) {
            tests = new BenchmarkTreeSets(nThreads, durationSeconds*1000, csvdb, 100);
            System.gc();
            Thread.sleep(sleepGCMiliSeconds); // sleep for a while second to allow the GC to work a bit                
            tests.performanceTests(nThreads, numRuns);
        }  // 10%

        for (Integer nThreads : threadList) {
            tests = new BenchmarkTreeSets(nThreads, durationSeconds*1000, csvdb, 10);
            System.gc();
            Thread.sleep(sleepGCMiliSeconds); // sleep for a while second to allow the GC to work a bit                
            tests.performanceTests(nThreads, numRuns);
        }   // 1%
        
        for (Integer nThreads : threadList) {
            tests = new BenchmarkTreeSets(nThreads, durationSeconds*1000, csvdb, 1);
            System.gc();
            Thread.sleep(sleepGCMiliSeconds); // sleep for a while second to allow the GC to work a bit                
            tests.performanceTests(nThreads, numRuns);
        }    // 0.1%
        
        for (Integer nThreads : threadList) {
            tests = new BenchmarkTreeSets(nThreads, durationSeconds*1000, csvdb, 0);
            System.gc();
            Thread.sleep(sleepGCMiliSeconds); // sleep for a while second to allow the GC to work a bit                
            tests.performanceTests(nThreads, numRuns);
        }    // 0.0%
        
        // In the end, print the results for the medians
        System.out.println("\n\n\n\n\n #---#---#---#---#   Showing all runs   #---#---#---#---#");
        System.out.println("Duration of each run in seconds = "+durationSeconds);
        System.out.println("Number of elements = "+numElements);
        csvdb.saveDB(BenchmarkTreeSets.numElements);        
    }
    

    /**
     * Execute latency benchmarks
     * Make sure to use 10k elements
     */
    static void latencyBenchmark() throws InterruptedException {
        final int durationSeconds = 1000;      // 1000  seconds (16 minutes) is the default for the paper
        final CSVDatabase csvdb = new CSVDatabase();      
        final int sleepGCMiliSeconds = 5000; // Number of miliseconds to sleep between tests for the GC to enter 
        System.out.println("This system has " + Runtime.getRuntime().availableProcessors() + " cores");

        BenchmarkTreeSets tests = new BenchmarkTreeSets(16, durationSeconds*1000, csvdb, 1000);
        System.gc();
        Thread.sleep(sleepGCMiliSeconds); // sleep for a while second to allow the GC to work a bit
        tests.singleLatencyTest(TestCase.CopyOnWriteCASTreeSet);
        System.gc();
        Thread.sleep(sleepGCMiliSeconds);
        tests.singleLatencyTest(TestCase.CopyOnWriteLockTreeSet);
        System.gc();
        Thread.sleep(sleepGCMiliSeconds);
        tests.singleLatencyTest(TestCase.CopyOnWriteMQLFTreeSet);
        System.gc();
        Thread.sleep(sleepGCMiliSeconds);
        tests.singleLatencyTest(TestCase.CopyOnWriteMQWFTreeSet);
    }
    
    
    /**
     * @param args
     */
    public static void main(String[] args)  throws InterruptedException {
        throughputBenchmark();
        //latencyBenchmark();
    }
       
}
