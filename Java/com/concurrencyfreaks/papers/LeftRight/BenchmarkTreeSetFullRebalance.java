package com.concurrencyfreaks.papers.LeftRight;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.concurrent.atomic.AtomicLongArray;

import com.concurrencyfreaks.tests.CSVDatabase.NumOps;
import com.concurrencyfreaks.tests.CSVDatabase;

import edu.stanford.ppl.concurrent.SnapTreeMap;


public class BenchmarkTreeSetFullRebalance {
    /*
     * Testing parameters
     */
    private int numElements = 1000;                     // 1000 elements in the data-structure
    private final long numIterations = 10000000000L;    // number of iterations
    private final int numMilis = 10*1000;               // 10 seconds
    private final long[][] index = new long[MAX_NUM_THREADS][TestCase.MAX_TEST_CASES.ordinal()];

	enum TestCase {
        LRScalableTreeSet,
        LRScalableTreeSetOptimistic, // Uses the LRScalableTreeSet but calls optimisticContains() contains()  
        LRScalableTreeSetNV,  
        LRScalableTreeSetRV,         // A TreeSet where each ReaderState has its own version
        LRSkipList,
        ScalableRWLockTreeSet,       // A TreeSet that uses ScalableRWLock to protect its access
        SnapTreeMap,                 // A TreeMap that uses hand-over-hand locking
        MAX_TEST_CASES,
	}


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

    /*
     * Data structures that will be tested in this test-suite
     */
    private final LRScalableTreeSet<UserData> lrScalableTreeSet = new LRScalableTreeSet<UserData>();
    private final LRScalableTreeSet<UserData> lrScalableTreeSetOptimistic = new LRScalableTreeSet<UserData>();
    private final LRScalableTreeSetNV<UserData> lrScalableTreeSetNV= new LRScalableTreeSetNV<UserData>();
    private final LRScalableTreeSetRV<UserData> lrScalableTreeSetRV = new LRScalableTreeSetRV<UserData>();
    private final SnapTreeMap<UserData, UserData> snapTreeMap = new SnapTreeMap<UserData, UserData>();
    private final ScalableRWLockTreeSet<UserData> rwLockTreeSet = new ScalableRWLockTreeSet<UserData>(); 

    
    private final static int MAX_NUM_THREADS = 256; // Only used in WorkerThread array

    private final AtomicLongArray globalIndex = new AtomicLongArray(TestCase.MAX_TEST_CASES.ordinal());
    UserData[] udarray = new UserData[numElements*4];
    WorkerThread[] workerThreads = new WorkerThread[MAX_NUM_THREADS];
    static CSVDatabase csv = new CSVDatabase();

    // Extra constructor that allows to change the number of elements
    BenchmarkTreeSetFullRebalance(int numElements) {
        this.numElements = numElements;
        udarray = new UserData[numElements*4];
        for (int i = 0; i < TestCase.MAX_TEST_CASES.ordinal(); i++) {
            globalIndex.set(i, numElements);
            for(int j = 0; j < MAX_NUM_THREADS; j++){
                index[j][i] = numElements+j;
            }
        }
    }
    BenchmarkTreeSetFullRebalance() {
        for (int i = 0; i < TestCase.MAX_TEST_CASES.ordinal(); i++) {
            globalIndex.set(i, this.numElements);
            for(int j = 0; j < MAX_NUM_THREADS; j++){
                index[j][i] = numElements+j;
            }
        }
    }
    
    /**
     * Inner class for the Worker thread
     */
    class WorkerThread extends Thread {

        volatile boolean quit = false;
        long numOps = 0;
        long numReadOps = 0;
        long numWriteOps = 0;
        int writePerMil;
        TestCase stype;
        int tidx;
        int numThreads;

        WorkerThread(int writePerMil, TestCase stype, int tidx, int numThreads) {
            this.writePerMil = writePerMil;
            this.stype = stype;
            this.tidx = tidx;
            this.numThreads = numThreads;
        }
        
        /**
         * An imprecise but fast random number generator
         */
        public long randomLong(long x) {
            x ^= (x << 21);
            x ^= (x >>> 35);
            x ^= (x << 4);
            return x;
        }
        
        public void run() {
            long xrand = System.nanoTime();
            long x;

            for (long i = 0; i < numIterations; i++) {
                if (quit) return;

                xrand = randomLong(xrand);
                if (xrand < 0) x = -xrand;
                else x = xrand;
                
                if (writePerMil != 1000 && (writePerMil == 0 || ((x%1000) >= writePerMil))) {
                    int i0 = (int)(i % (numElements*4));
                    int i1 = (int)((i+1) % (numElements*4));
                    final UserData ud0 = udarray[i0];
                    final UserData ud1 = udarray[i1];
                    switch (stype) {
                        case LRScalableTreeSetNV:
                        	lrScalableTreeSetNV.contains(ud0);
                        	lrScalableTreeSetNV.contains(ud1);
                            break;
                        case LRScalableTreeSet:
                            lrScalableTreeSet.contains(ud0);
                            lrScalableTreeSet.contains(ud1);
                            break;
                        case LRScalableTreeSetOptimistic:
                            lrScalableTreeSetOptimistic.optimisticContains(ud0);
                            lrScalableTreeSetOptimistic.optimisticContains(ud1);
                            break;
                        case LRScalableTreeSetRV:                            	
                            lrScalableTreeSetRV.contains(ud0);
                            lrScalableTreeSetRV.contains(ud1);
                            break;
                        case SnapTreeMap:                            	
                            snapTreeMap.containsKey(ud0);
                            snapTreeMap.containsKey(ud1);
                            break;
                        case ScalableRWLockTreeSet:
                        	rwLockTreeSet.containsKey(ud0);
                        	rwLockTreeSet.containsKey(ud1);
                        	break;
                    }
                    numReadOps += 2;
                    numOps += 2;
                } else {
                    long elem = index[tidx][stype.ordinal()];
                    int iAdd = (int)((index[tidx][stype.ordinal()]) % (numElements*4));
                    int iRemove = (int)((index[tidx][stype.ordinal()] - numElements) % (numElements*4));
                    index[tidx][stype.ordinal()] += numThreads;
                    
                    final UserData udAdd = udarray[iAdd];
                    final UserData udRemove = udarray[iRemove];
                    boolean isRemoved = false;
                    boolean isAdded = false;
                    
                    switch (stype) {
                        case LRScalableTreeSetNV:
                        	isRemoved = lrScalableTreeSetNV.remove(udRemove); 
                        	isAdded = lrScalableTreeSetNV.add(udAdd); 
                            break;
                        case LRScalableTreeSet:
                            isRemoved = lrScalableTreeSet.remove(udRemove);
                            isAdded = lrScalableTreeSet.add(udAdd);
                            break;
                        case LRScalableTreeSetOptimistic:
                            isRemoved = lrScalableTreeSetOptimistic.remove(udRemove);
                            isAdded = lrScalableTreeSetOptimistic.add(udAdd);
                            break;
                        case LRScalableTreeSetRV:
                            isRemoved = lrScalableTreeSetRV.remove(udRemove);
                            isAdded = lrScalableTreeSetRV.add(udAdd);
                            break;
                        case SnapTreeMap:
                            isRemoved = (snapTreeMap.remove(udRemove) != null);
                            isAdded = (snapTreeMap.put(udAdd, udAdd) == null);
                            break;
                        case ScalableRWLockTreeSet:
                            isRemoved = rwLockTreeSet.remove(udRemove);
                        	isAdded = rwLockTreeSet.add(udAdd);
                        	break;
                    }
                    //if (!isRemoved) System.out.println(elem+" isRemoved Error with iAdd="+iAdd+" iRemove="+iRemove+" tidx="+tidx);
                    //if (!isAdded) System.out.println(elem+" isAdded Error with iAdd="+iAdd+" iRemove="+iRemove+" tidx="+tidx);
                    numWriteOps += 2;
                    numOps += 2;
                }                    
            }
        }
    }

    private void myinit() {

        // fill in the full array, each entry with a unique entry
        for (int i = 0; i < numElements*4; i++) {
            udarray[i] = new UserData();
            udarray[i].a = i;
            udarray[i].b = i % 7;
        }

        System.out.println("Filling up trees with "+numElements+" elements...");
        // Insert only the first block of elements
        for (int i = 0; i < numElements; i++) lrScalableTreeSet.add(udarray[i]);
        for (int i = 0; i < numElements; i++) lrScalableTreeSetNV.add(udarray[i]);
        for (int i = 0; i < numElements; i++) lrScalableTreeSetRV.add(udarray[i]);
        for (int i = 0; i < numElements; i++) lrScalableTreeSetOptimistic.add(udarray[i]);
        for (int i = 0; i < numElements; i++) snapTreeMap.put(udarray[i], udarray[i]);
        for (int i = 0; i < numElements; i++) rwLockTreeSet.add(udarray[i]);
    }

    
    /**
     * Runs all the dedicated tests
     */
    private void allDedicatedWriterTests(int numThreads) {
        workerThreads = new WorkerThread[numThreads];
        
        System.out.println("----- Dedicated Writer tests (2 Writers + " + (numThreads - 2) + " Readers) numElements="+numElements+" -----");
        dedicatedWriterTest(numThreads, TestCase.LRScalableTreeSet);
        dedicatedWriterTest(numThreads, TestCase.LRScalableTreeSetNV);
        dedicatedWriterTest(numThreads, TestCase.LRScalableTreeSetRV);
        dedicatedWriterTest(numThreads, TestCase.LRScalableTreeSetOptimistic);        
        dedicatedWriterTest(numThreads, TestCase.SnapTreeMap);
        dedicatedWriterTest(numThreads, TestCase.ScalableRWLockTreeSet);        
        System.out.println("");
    }
    

    /**
     * Does tests with 50% Writes, or 1% Writes, or 0.1% Writes, etc
     */
    private void allWritePerMilTests(int numThreads, int writePerMil) {
        workerThreads = new WorkerThread[numThreads];
        double writePercentage = writePerMil == 0 ? 0 : writePerMil/10.;
        System.out.println("----- writePerMil tests numThreads=" +numThreads+"  Writes="+writePercentage+"%  numElements="+numElements+" -----");        
        writePerMilTest(numThreads, writePerMil, TestCase.LRScalableTreeSet);
        writePerMilTest(numThreads, writePerMil, TestCase.LRScalableTreeSetNV);
        writePerMilTest(numThreads, writePerMil, TestCase.LRScalableTreeSetRV);
        writePerMilTest(numThreads, writePerMil, TestCase.LRScalableTreeSetOptimistic);        
        writePerMilTest(numThreads, writePerMil, TestCase.SnapTreeMap);        
        writePerMilTest(numThreads, writePerMil, TestCase.ScalableRWLockTreeSet);
        System.out.println("");
    }

    
    private void powerPCWritePerMilTests(int numThreads, int writePerMil) {
        workerThreads = new WorkerThread[numThreads];
        double writePercentage = writePerMil == 0 ? 0 : writePerMil/10.;
        System.out.println("----- writePerMil tests numThreads=" +numThreads+"  Writes="+writePercentage+"%  numElements="+numElements+" -----");        
        writePerMilTest(numThreads, writePerMil, TestCase.LRScalableTreeSet);       
        writePerMilTest(numThreads, writePerMil, TestCase.SnapTreeMap);        
        System.out.println("");
    }
    
    
    private void sleepAndStopWorkerThreads(int numThreads) {
        try {
            Thread.sleep(numMilis);
        } catch (InterruptedException e) {
            System.out.println("InterruptedException");
        }
        for (int i = 0; i < numThreads; i++) {
            workerThreads[i].quit = true;
        }    	
    }

    
    private void dedicatedWriterTest(int numThreads, TestCase stype) {
        // If we see an error here just increase the number of spaces
        String indentedName = stype.toString() + "                              ".substring(stype.toString().length());
        System.out.print("##### "+indentedName+" #####  ");
        
        // Create the threads and then start them all in one go
        for (int i = 0; i < numThreads; i++) {
            if (i == 0 || i == 1) {
                // First and second threads are Writers
                workerThreads[i] = new WorkerThread(1000, stype, i, 2);
            } else {
                // all other threads are Readers
                workerThreads[i] = new WorkerThread(0, stype, i, 2);
            }
        }
        for (int i = 0; i < numThreads; i++) {
            workerThreads[i].start();
        }

        sleepAndStopWorkerThreads(numThreads);

        long totalOps = 0;
        long totalReadOps = 0;
        long totalWriteOps = 0;
        for (int i = 0; i < numThreads; i++) {
            totalOps += workerThreads[i].numOps;
            totalReadOps += workerThreads[i].numReadOps;
            totalWriteOps += workerThreads[i].numWriteOps;
        }

        try {
            for (int i = 0; i < numThreads; i++) {
                workerThreads[i].join();
            }
        } catch (InterruptedException e) {
            System.out.println("InterruptedException");
        }
        System.out.println("Total Ops/ms = " + (totalOps /numMilis) + "   Reads/ms = " + (totalReadOps/numMilis) + "   Writes/ms = " + (totalWriteOps/numMilis));
        csv.addRunDedicated(stype.name(), numThreads-2, 2, totalReadOps/numMilis, totalWriteOps/numMilis);
    }

    
    private void writePerMilTest(int numThreads, int writePerMil, TestCase stype) {
        // If we see an error here just increase the number of spaces
        String indentedName = stype.toString() + "                              ".substring(stype.toString().length());
        System.out.print("##### "+indentedName+" #####  ");
        
        // Create the threads and then start them all in one go
        for (int i = 0; i < numThreads; i++) {
            workerThreads[i] = new WorkerThread(writePerMil, stype, i, numThreads);
        }
        for (int i = 0; i < numThreads; i++) {
            workerThreads[i].start();
        }

        sleepAndStopWorkerThreads(numThreads);

        long totalOps = 0;
        for (int i = 0; i < numThreads; i++) {
            totalOps += workerThreads[i].numOps;
        }

        try {
            for (int i = 0; i < numThreads; i++) {
                workerThreads[i].join();
            }
        } catch (InterruptedException e) {
            System.out.println("InterruptedException");
        }
        System.out.println("Total Ops/ms = " + (totalOps /numMilis));
        csv.addRun(stype.name(), writePerMil, numThreads, totalOps/numMilis);
    }
        
        
    /**
     * This is the performance test-suite that is supposed to be run on the 
     * Opteron with 32 cores, to put on the paper
     */
    static void performanceTest32Cores() {        
        LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(2, 3, 4, 6, 8, 12, 16, 20, 24, 28, 32));
        
        // Do a test run for 1K elements or 1M elements
        BenchmarkTreeSetFullRebalance tests = new BenchmarkTreeSetFullRebalance(1000*1);
        tests.myinit();
          
        System.out.println("This system has " + Runtime.getRuntime().availableProcessors() + " cores");
        for (Integer nThreads : threadList) {
            tests.allDedicatedWriterTests(nThreads);
        }
        // Save the results of the test runs in .csv format to import in Excel
        tests.csv.saveDBDedicated(NumOps.TotalOps, tests.numElements);
        tests.csv.saveDBDedicated(NumOps.ReadOps,  tests.numElements);
        tests.csv.saveDBDedicated(NumOps.WriteOps, tests.numElements);  
    }
    
    
    /**
     * Modify this if you want to do just a run
     */
    static void simpleDedicateTest() {
        LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(4));
        
        BenchmarkTreeSetFullRebalance tests = new BenchmarkTreeSetFullRebalance();
        tests.myinit();
          
        System.out.println("This system has " + Runtime.getRuntime().availableProcessors() + " cores");
        for (Integer nThreads : threadList) {
            tests.allDedicatedWriterTests(nThreads);
        }
    }

    
    /**
     * Mixed task tests, where each thread may do either Writes or Reads
     */
    static void simpleWritePerMilTest() {
        LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(1, 2, 3, 4, 6, 8, 12, 16, 20, 24, 28, 32));
        //LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(4));
        
        BenchmarkTreeSetFullRebalance tests = new BenchmarkTreeSetFullRebalance();
        // 1K elements or 1M elements
        int elems = 1000*1; 
        System.out.println("This system has " + Runtime.getRuntime().availableProcessors() + " cores");
        
        for (Integer nThreads : threadList) {
            int calc = elems/nThreads;
            tests = new BenchmarkTreeSetFullRebalance(calc*nThreads);
            tests.myinit();
            tests.allWritePerMilTests(nThreads, 100);
        }  // 10%
        
        for (Integer nThreads : threadList){
            int calc = elems/nThreads;
            tests = new BenchmarkTreeSetFullRebalance(calc*nThreads);
            tests.myinit();
            tests.allWritePerMilTests(nThreads, 10);
        }   // 1%
        
        for (Integer nThreads : threadList){
            int calc = elems/nThreads;
            tests = new BenchmarkTreeSetFullRebalance(calc*nThreads);
            tests.myinit();
            tests.allWritePerMilTests(nThreads, 1);
        }    // 0.1%
        
        tests.csv.saveDB(tests.numElements);
    }

    
    /**
     * Mixed task benchmarks be run on PwerPC 176 threads of RunAbove cloud
     * Notice that on those instances we don't have a proper JDK 8 so we 
     * better comment out every usage of StampedLock otherwise it won't run.
     */
    static void mixedTestPowerPCTest() {
        LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(1, 2, 3, 4, 8, 16, 32, 64, 128, 176));
        //LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(8));
        
        BenchmarkTreeSetFullRebalance tests = new BenchmarkTreeSetFullRebalance();
        // 1K elements or 1M elements
        int elems = 1000*1; 
        System.out.println("This system has " + Runtime.getRuntime().availableProcessors() + " cores");
        
        for (Integer nThreads : threadList) {
            int calc = elems/nThreads;
            tests = new BenchmarkTreeSetFullRebalance(calc*nThreads);
            tests.myinit();
            tests.powerPCWritePerMilTests(nThreads, 100);
        }  // 10%
        
        for (Integer nThreads : threadList){
            int calc = elems/nThreads;
            tests = new BenchmarkTreeSetFullRebalance(calc*nThreads);
            tests.myinit();
            tests.powerPCWritePerMilTests(nThreads, 10);
        }   // 1%
        
        for (Integer nThreads : threadList){
            int calc = elems/nThreads;
            tests = new BenchmarkTreeSetFullRebalance(calc*nThreads);
            tests.myinit();
            tests.powerPCWritePerMilTests(nThreads, 1);
        }    // 0.1%
        
        tests.csv.saveDB(tests.numElements);
    }
    
    
    public static void main(String[] args) {
        //performanceTest32Cores(); 
        //simpleWritePerMilTest();
        //simpleDedicateTest();       
        mixedTestPowerPCTest();
    }
}
