package com.concurrencyfreaks.papers.LeftRight;

import java.util.concurrent.atomic.AtomicLong;
import com.concurrencyfreaks.papers.LeftRight.LRScalableTreeSet;
import edu.stanford.ppl.concurrent.SnapTreeMap;



public class BenchmarkTreeSetLatency {
    
    public enum TestCase {
        SnapTreeMap,
        ScalableRWLockTreeSet,   
        LRScalableTreeSet,
        LRScalableTreeSetOptimistic,
        MAX_TEST_CASES,
    }
        
    
    // Instance variables
    private final LRScalableTreeSet<UserData> lrScalableTreeSet = new LRScalableTreeSet<UserData>();
    private final LRScalableTreeSet<UserData> lrScalableTreeSetOptimistic = new LRScalableTreeSet<UserData>();
    private final ScalableRWLockTreeSet<UserData> scalableRWLockTreeSet = new ScalableRWLockTreeSet<UserData>(); 
    private final SnapTreeMap<UserData, UserData> snapTreeMap = new SnapTreeMap<UserData, UserData>();
    
    /*
     * Testing parameters
     */
    private int numElements = 0;                       // elements in the data-structure (set in main())
    private final long numMilis = 10000*1000;          // 10k seconds (for the paper)
    private final long numIterations = 100000000000L;  // number of iterations   
    
    private final static int MAX_NUM_THREADS = 128;       // Only used in WorkerThread array
    private final static int MAX_LINEAR_HIST_BINS = 1000; // 1000 when in doubt, but 200 is usually enough
    private final static int LINEAR_BIN_WIDTH = 1000;     // width of bins is 1000 nanosecond (1 micro-second)

    private final long[] oddIndex = new long[TestCase.MAX_TEST_CASES.ordinal()];
    private final long[] evenIndex = new long[TestCase.MAX_TEST_CASES.ordinal()];    
    UserData[] udarray = new UserData[numElements*4];
    WorkerThread[] workerThreads = new WorkerThread[MAX_NUM_THREADS];
    final AtomicLong heartBeat = new AtomicLong(0);
    
    /**
     * Inner class for user's data that will be used as "Value" of the HashMap
     */
    public class UserData implements Comparable<UserData> {
        public int a = 1;
        public int b = 2;
        public int compareTo(UserData o){
            return this.a - o.a;
        }
    } 
    
    /**
     * Default construtor
     * @param numElements
     */
    public BenchmarkTreeSetLatency(int numElements) {
        this.numElements = numElements;
        this.udarray = new UserData[numElements*4];
    }
    

    /**
     * Inner class for the heartbeat thread
     */
    class HeartBeatThread extends Thread {
        volatile boolean quit = false;
        public void run() {
            while (!quit) {
                heartBeat.getAndAdd(1);
            }
        }
    }
    
    
    /**
     * Inner class for the Worker thread
     */
    class WorkerThread extends Thread {

        volatile boolean quit = false;
        volatile boolean doMeasures = false;
        long numOps = 0;
        long numReadOps = 0;
        long numWriteOps = 0;
        long numHeartBeats = 0;
        int writePerMil;
        TestCase stype;
        int tidx;        
        final long[] readLatencyLinearHistogram = new long[MAX_LINEAR_HIST_BINS];

        WorkerThread(int writePerMil, TestCase stype, int tidx) {
            this.writePerMil = writePerMil;
            this.stype = stype;
            this.tidx = tidx;
        }
        
        public void run() {

            for (long i = 0; i < numIterations; i++) {
                if (quit) return; 

                if (writePerMil != 1000 && (writePerMil == 0 || (i % 1000 > writePerMil))) {
                    final int i0 = (int)(i % (numElements*4));
                    final UserData ud0 = udarray[i0];
                    
                    long startBeats = System.nanoTime();
                    //long startBeats = heartBeat.get();
                    switch (stype) {
                        case LRScalableTreeSet:                             
                            lrScalableTreeSet.contains(ud0);
                            break;
                        case LRScalableTreeSetOptimistic:                             
                            lrScalableTreeSetOptimistic.optimisticContains(ud0);
                            break;
                        case SnapTreeMap:                    
                            snapTreeMap.containsKey(ud0);
                            break;
                        case ScalableRWLockTreeSet:
                            scalableRWLockTreeSet.containsKey(ud0);
                            break;
                    }
                    long stopBeats = System.nanoTime();
                    //long stopBeats = heartBeat.get();
                    final long numbeats = stopBeats-startBeats;   
                    if (doMeasures) {
                        storeLinearLatency(numbeats);
                        numHeartBeats += numbeats;                        
                        numReadOps += 1;
                        numOps += 1;
                    }
                } else {
                    int iAdd, iRemove;
                    if (tidx == 0) { // tidx 0 and 1 are always Writers
                        iAdd = (int)((evenIndex[stype.ordinal()]) % (numElements*4));
                        iRemove = (int)((evenIndex[stype.ordinal()] - numElements) % (numElements*4));
                        evenIndex[stype.ordinal()] += 2;                        
                    } else {
                        iAdd = (int)((oddIndex[stype.ordinal()]) % (numElements*4));
                        iRemove = (int)((oddIndex[stype.ordinal()] - numElements) % (numElements*4));
                        oddIndex[stype.ordinal()] += 2;                        
                    }
                    UserData udAdd = udarray[iAdd];
                    UserData udRemove = udarray[iRemove];
                    boolean isRemoved = false;
                    boolean isAdded = false;
                    
                    switch (stype) {
                        case LRScalableTreeSet:
                            isRemoved = lrScalableTreeSet.remove(udRemove);
                            isAdded = lrScalableTreeSet.add(udAdd);
                            break;
                        case LRScalableTreeSetOptimistic:
                            isRemoved = lrScalableTreeSetOptimistic.remove(udRemove);
                            isAdded = lrScalableTreeSetOptimistic.add(udAdd);
                            break;
                        case SnapTreeMap:
                            isRemoved = (snapTreeMap.remove(udRemove) != null);
                            isAdded = (snapTreeMap.put(udAdd, udAdd) == null);
                            break;
                        case ScalableRWLockTreeSet:
                            isRemoved = scalableRWLockTreeSet.remove(udRemove);
                            isAdded = scalableRWLockTreeSet.add(udAdd);
                            break;
                    }
                    if (!isRemoved) System.out.println("isRemoved Error with iAdd="+iAdd+" iRemove="+iRemove+" tidx="+tidx);
                    if (!isAdded) System.out.println("isAdded Error with iAdd="+iAdd+" iRemove="+iRemove+" tidx="+tidx);                        
                    numWriteOps += 2;
                    numOps += 2;
                }                    
            }
        }
                
        private void storeLinearLatency(long delta) {
            int ibox = (int)(delta/LINEAR_BIN_WIDTH);
            if (ibox > readLatencyLinearHistogram.length-1) ibox = readLatencyLinearHistogram.length-1;
            readLatencyLinearHistogram[ibox]++;
        }
        
        private void printNonZeroLinearHistogram() {
            System.out.print("Linear histo = [");
            for (int ibox = 0; ibox < readLatencyLinearHistogram.length-1; ibox++) {
                if (readLatencyLinearHistogram[ibox] > 0) System.out.print("X");
                else System.out.print(" ");
            }
            System.out.println("]");
        }
        
        /*
         * If percentage is 0.99 it means 99%
         */
        public double computeVAR(double percentage) {
            final long cutoff = (long)(numReadOps*percentage);
            long sum = 0;
            for (int ibox=0; ibox < readLatencyLinearHistogram.length; ibox++) {
                sum += readLatencyLinearHistogram[ibox];
                //System.out.println("sum = "+sum+"  cutoff = "+cutoff);
                if (sum >= cutoff) {
                    System.out.println("Latency at "+percentage*100+"% = "+ibox*1000./LINEAR_BIN_WIDTH+" micro-seconds");
                    return 0;
                }
            }
            System.out.println("Latency at "+percentage*100+"% = ???");
            return 0;
        }
    }
    

    private void myinit() {
        for (int i = 0; i < numElements*4; i++) {
            udarray[i] = new UserData();
            udarray[i].a = i;
            udarray[i].b = i % 7;
        }
        for (int i = 0; i < TestCase.MAX_TEST_CASES.ordinal(); i++) {
            oddIndex[i] = numElements+1;
            evenIndex[i] = numElements;
        }

        System.out.println("Filling up trees with "+numElements+" elements...");
        for (int i = 0; i < numElements; i++) lrScalableTreeSet.add(udarray[i]);
        for (int i = 0; i < numElements; i++) lrScalableTreeSetOptimistic.add(udarray[i]);
        for (int i = 0; i < numElements; i++) snapTreeMap.put(udarray[i], udarray[i]);
        for (int i = 0; i < numElements; i++) scalableRWLockTreeSet.add(udarray[i]);
    }
    
    
    public void dedicatedWriterTest(int numThreads, TestCase stype) {
        // If we see an error here just increase the number of spaces
        String indentedName = stype.toString() + "                              ".substring(stype.toString().length());
        System.out.print("##### "+indentedName+" #####  ");
        
        // Create the Worker threads threads and the hearbeat thread
        for (int i = 0; i < numThreads; i++) {
            if (i == 0 || i == 1) {
                // The first two threads are Writers
                workerThreads[i] = new WorkerThread(1000, stype, i);
            } else {
                workerThreads[i] = new WorkerThread(0, stype, i);
            }
        }
        HeartBeatThread hb = new HeartBeatThread();
        hb.start();
        
        // Sleep for 0.1 seconds before starting the worker threads
        try {
            Thread.sleep(100);
        } catch(InterruptedException e){
            System.out.println("InterruptedException");
        }
        // Start the worker threads
        for (int i = 0; i < numThreads; i++) workerThreads[i].start();

        // The first 20 seconds we don't measure anything
        try {
            Thread.sleep(20*1000);
        } catch(InterruptedException e){
            System.out.println("InterruptedException");
        }
        // Start the measurements
        for (int i = 0; i < numThreads; i++) workerThreads[i].doMeasures = true;

        // Sleep for 10k seconds
        try {
            Thread.sleep(numMilis);
        } catch(InterruptedException e){
            System.out.println("InterruptedException");
        }
        // Stop the worker threads
        for (int i = 0; i < numThreads; i++) workerThreads[i].quit = true;
        
        long totalReadOps = 0;
        for (int i = 0; i < numThreads; i++) {
            totalReadOps += workerThreads[i].numReadOps;
        }
        
        try {
            for (int i = 0; i < numThreads; i++) workerThreads[i].join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        // Stop and join the hearbeat thread
        hb.quit = true;
        try {
            hb.join();
        } catch (InterruptedException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        
        System.out.println("Read Ops/ms = "+(totalReadOps/numMilis)+" total read ops = "+totalReadOps);

        // Compute the mean by summing over all Reader threads
        long sumHeartBeats = 0;
        for (int i = 0; i < numThreads; i++) {
            sumHeartBeats += workerThreads[i].numHeartBeats;
        }
        double distributionMean = (double)sumHeartBeats/(double)totalReadOps;
        System.out.println("distributionMean = "+distributionMean);
        
        /*
        workerThreads[2].computeVAR(0.99);
        workerThreads[2].computeVAR(0.999);
        workerThreads[2].computeVAR(0.9999);
        workerThreads[2].computeVAR(0.99999);
        */
        
        // Aggregate the histograms from all the Reader threads
        final double[] readLatencyLinearHistogram = new double[MAX_LINEAR_HIST_BINS];
        for (int i = 0; i < numThreads; i++) {
            if (i == 0 || i == 1) continue;
            for (int ibox = 0 ; ibox < readLatencyLinearHistogram.length; ibox++) {
                readLatencyLinearHistogram[ibox] += workerThreads[i].readLatencyLinearHistogram[ibox];
            }            
        }
        // Normalize the histograms
        double normFactor = 0;
        for (int ibox = 0 ; ibox < readLatencyLinearHistogram.length; ibox++) {
            normFactor += readLatencyLinearHistogram[ibox];
        }
        for (int ibox = 0 ; ibox < readLatencyLinearHistogram.length; ibox++) {
            readLatencyLinearHistogram[ibox] /= normFactor;
        }
        // Show the histograms in csv format
        for (int ibox = 0 ; ibox < readLatencyLinearHistogram.length; ibox++) {
            System.out.println(ibox+", "+readLatencyLinearHistogram[ibox]);
        }

        System.out.println();
                
    }
    
    

    
    /**
     * @param args
     */
    public static void main(String[] args) {
        final int numThreads = 4;
        final int numElements = 1000*1000;
                
        BenchmarkTreeSetLatency tests = new BenchmarkTreeSetLatency(numElements);
        tests.myinit();
        System.out.println("This system has " + Runtime.getRuntime().availableProcessors() + " cores");
        System.out.println("Set the javaw.exe to high priority and run the JVM with -XX:+CMSIncrementalMode before running this test suite");
        
        tests.dedicatedWriterTest(numThreads, TestCase.ScalableRWLockTreeSet);
        tests.dedicatedWriterTest(numThreads, TestCase.LRScalableTreeSet);
        tests.dedicatedWriterTest(numThreads, TestCase.LRScalableTreeSetOptimistic);
        tests.dedicatedWriterTest(numThreads, TestCase.SnapTreeMap);
    }
   

        
}
