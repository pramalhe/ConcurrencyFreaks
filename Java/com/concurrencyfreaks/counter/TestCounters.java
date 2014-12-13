package com.concurrencyfreaks.counter;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.concurrent.atomic.AtomicLong;
import java.util.concurrent.atomic.LongAdder;

import org.cliffc.high_scale_lib.ConcurrentAutoTable;

import com.concurrencyfreaks.jdkext.LongAdderCVS;
import com.concurrencyfreaks.jdkext.LongAdderRelax;


public class TestCounters {
    
    public enum TestCase {
        AtomicLong,
        LongAdder,
        LongAdderCVS,
        LongAdderRelax,
        OffByXCounter16,
        OffByXCounter64,
        DistributedCacheLineCounter,
        DistributedCacheLineCounterRelax,
        ConcurrentAutoTable,
    }
    
    public enum TestKind {
        Mixed_Readers_and_Writers,
        Single_Writer_Multiple_Readers,
        Single_Reader_Multiple_Writers,
    }

    private final int numMilis = 10000;             // 10 seconds
    private final static int MAX_NUM_THREADS = 1024;
    private final static boolean WORKER_READER = true;
    private final static boolean WORKER_WRITER = false;
    
    // References to counters
    private AtomicLong atomicCounter;
    private LongAdder longAdder;
    private LongAdderCVS longAdderCVS;
    private LongAdderRelax longAdderRelax;
    private OffByXCounter offByXCounter16;
    private OffByXCounter offByXCounter64;
    private DistributedCacheLineCounter dclc;
    private DistributedCacheLineCounterRelax dclcrelax;
    private ConcurrentAutoTable autoTable;
    
    
    WorkerThread[] workerThreads = new WorkerThread[MAX_NUM_THREADS];    
    
    
    public void doSingleTest(int numThreads, TestKind kind, TestCase stype) {
        // This will nullify previous instances
        atomicCounter = new AtomicLong(0);
        longAdder = new LongAdder();
        longAdderCVS = new LongAdderCVS();
        longAdderRelax = new LongAdderRelax();
        offByXCounter16 = new OffByXCounter(16);
        offByXCounter64 = new OffByXCounter(64);
        dclc = new DistributedCacheLineCounter();
        dclcrelax = new DistributedCacheLineCounterRelax();
        autoTable = new ConcurrentAutoTable();

        // If we see an error here just increase the number of spaces
        String indentedName = stype.toString() + "                                 ".substring(stype.toString().length());
        System.out.print("##### "+indentedName+" #####  ");

        for (int tid = 0; tid < numThreads; tid++) {        
            switch (kind) {
            case Mixed_Readers_and_Writers:
                if (tid%2 == 0) workerThreads[tid] = new WorkerThread(tid, stype, WORKER_WRITER);
                else workerThreads[tid] = new WorkerThread(tid, stype, WORKER_READER);
                break;
            case Single_Writer_Multiple_Readers:
                if (tid == 0) workerThreads[tid] = new WorkerThread(tid, stype, WORKER_WRITER);
                else workerThreads[tid] = new WorkerThread(tid, stype, WORKER_READER);
                break;
            case Single_Reader_Multiple_Writers:
                if (tid == 0) workerThreads[tid] = new WorkerThread(tid, stype, WORKER_READER);
                else workerThreads[tid] = new WorkerThread(tid, stype, WORKER_WRITER);
                break;
            }
            workerThreads[tid].start();
        }
        
        sleepAndStopWorkerThreads(numThreads);
        
        try {
            for (int i = 0; i < numThreads; i++) {
                workerThreads[i].join();
            }
        } catch (InterruptedException e) {
            System.out.println("InterruptedException");
        }    
        
        long totalNumGets = 0;
        long totalNumIncrements = 0;
        for (int i = 0; i < numThreads; i++) {
            totalNumGets += workerThreads[i].numGets;
            totalNumIncrements += workerThreads[i].numIncrements;
        }
        
        System.out.println("get()/ms = "+totalNumGets/numMilis+"    increment()/ms = "+totalNumIncrements/numMilis);
        
        if (stype == TestCase.LongAdderCVS) System.out.println("Cells.length = "+longAdderCVS.getCellsLength());
        if (stype == TestCase.LongAdderRelax) System.out.println("Cells.length = "+longAdderRelax.getCellsLength());
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
    
    
    public void doAllTests(int numThreads) {

        System.out.println("\nMixed Readers and Writers:");
        doSingleTest(numThreads, TestKind.Mixed_Readers_and_Writers, TestCase.AtomicLong);
        //doSingleTest(numThreads, TestKind.Mixed_Readers_and_Writers, TestCase.LongAdder);
        doSingleTest(numThreads, TestKind.Mixed_Readers_and_Writers, TestCase.LongAdderCVS);
        doSingleTest(numThreads, TestKind.Mixed_Readers_and_Writers, TestCase.LongAdderRelax);
        doSingleTest(numThreads, TestKind.Mixed_Readers_and_Writers, TestCase.DistributedCacheLineCounter);
        doSingleTest(numThreads, TestKind.Mixed_Readers_and_Writers, TestCase.DistributedCacheLineCounterRelax);
        //doSingleTest(numThreads, TestKind.Mixed_Readers_and_Writers, TestCase.ConcurrentAutoTable);

        System.out.println("\nSingle Reader:");
        doSingleTest(numThreads, TestKind.Single_Reader_Multiple_Writers, TestCase.AtomicLong);
        //doSingleTest(numThreads, TestKind.Single_Reader_Multiple_Writers, TestCase.LongAdder);
        doSingleTest(numThreads, TestKind.Single_Reader_Multiple_Writers, TestCase.LongAdderCVS);
        doSingleTest(numThreads, TestKind.Single_Reader_Multiple_Writers, TestCase.LongAdderRelax);
        doSingleTest(numThreads, TestKind.Single_Reader_Multiple_Writers, TestCase.DistributedCacheLineCounter);
        doSingleTest(numThreads, TestKind.Single_Reader_Multiple_Writers, TestCase.DistributedCacheLineCounterRelax);
        //doSingleTest(numThreads, TestKind.Single_Reader_Multiple_Writers, TestCase.ConcurrentAutoTable);
        
        /*
        System.out.println("\nSingle Writer:");
        doSingleTest(numThreads, TestKind.Single_Writer_Multiple_Readers, TestCase.AtomicLong);
        doSingleTest(numThreads, TestKind.Single_Writer_Multiple_Readers, TestCase.LongAdder);
        doSingleTest(numThreads, TestKind.Single_Writer_Multiple_Readers, TestCase.LongAdderCVS);
        doSingleTest(numThreads, TestKind.Single_Writer_Multiple_Readers, TestCase.LongAdderRelax);
        //doSingleTest(numThreads, TestKind.Single_Writer_Multiple_Readers, TestCase.OffByXCounter16);
        //doSingleTest(numThreads, TestKind.Single_Writer_Multiple_Readers, TestCase.OffByXCounter64);
        doSingleTest(numThreads, TestKind.Single_Writer_Multiple_Readers, TestCase.DistributedCacheLineCounter);
        doSingleTest(numThreads, TestKind.Single_Writer_Multiple_Readers, TestCase.DistributedCacheLineCounterRelax);
        //doSingleTest(numThreads, TestKind.Single_Writer_Multiple_Readers, TestCase.ConcurrentAutoTable);
        */
    }
    
    
    /**
     * Inner class for the Worker thread
     */
    class WorkerThread extends Thread {
        
        volatile boolean quit = false;
        boolean workerTask = WORKER_READER;
        TestCase stype;
        int tid;
        long numGets = 0;
        long numIncrements = 0;
        
        WorkerThread (int tid, TestCase stype, boolean task) {
            this.tid = tid;
            this.stype = stype;
            this.workerTask = task;
        }
        
        public void run() {
            numGets = 0;
            numIncrements = 0;

            while (!quit) {
                if (workerTask == WORKER_READER) {
                    switch (stype) {
                    case AtomicLong:
                        atomicCounter.get();
                        break;
                    case LongAdder:
                        longAdder.sum();
                        break;
                    case LongAdderCVS:
                        longAdderCVS.sum();
                        break;
                    case LongAdderRelax:
                        longAdderRelax.sum();
                        break;
                    case OffByXCounter16:
                        offByXCounter16.get();
                        break;
                    case OffByXCounter64:
                        offByXCounter64.get();
                        break;
                    case DistributedCacheLineCounter:
                        dclc.sum();
                        break;
                    case DistributedCacheLineCounterRelax:
                        dclcrelax.sum();
                        break;
                    case ConcurrentAutoTable:
                        autoTable.get();
                        break;
                    default:
                        numGets--;        
                    }
                    numGets++;
                } else {
                    // WORKER_WRITER
                    switch (stype) {
                    case AtomicLong:
                        atomicCounter.getAndIncrement();
                        break;
                    case LongAdder:
                        longAdder.increment();
                        break;
                    case LongAdderCVS:
                        longAdderCVS.increment();
                        break;
                    case LongAdderRelax:
                        longAdderRelax.increment();
                        break;
                    case OffByXCounter16:
                        // Only one thread can increment()
                        offByXCounter16.increment();
                        break;
                    case OffByXCounter64:
                        // Only one thread can increment()
                        offByXCounter64.increment();
                        break;
                    case DistributedCacheLineCounter:
                        dclc.increment();
                        break;
                    case DistributedCacheLineCounterRelax:
                        dclcrelax.increment();
                        break;
                    case ConcurrentAutoTable:
                        autoTable.increment();
                        break;                        
                    default:
                        numIncrements--;        
                    }
                    numIncrements++;
                }                
            }
        }
    }
    
    public static void main(String[] args) {
        TestCounters tcounters = new TestCounters();
        //LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(1, 2, 4, 8, 16, 24, 32, 64));
        //LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(1, 4, 16, 64, 128, 176));
        //LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(4));
        LinkedList<Integer> threadList = new LinkedList<Integer>(Arrays.asList(1, 2, 4, 6, 8, 16));
        for (Integer nThreads : threadList) {
            System.out.println("***** Running test with nThreads="+nThreads+" *****");
            tcounters.doAllTests(nThreads);
        }
        
    }
}
