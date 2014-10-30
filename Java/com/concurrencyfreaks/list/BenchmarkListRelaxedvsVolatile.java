package com.concurrencyfreaks.list;

import java.util.Arrays;
import java.util.LinkedList;
import java.util.concurrent.ConcurrentLinkedQueue;
import com.concurrencyfreaks.tests.CSVDatabase;



/**
 * This is a performance benchmark of lists using relaxed atomics vs volatile (sequential consistent)
 * 
 * It has Mixed mode only
 * 
 * Mixed:
 * Uses write-per-mil to decide if a randomly chosen element is going to
 * be added()/removed() or contains().
 * 
 */
public class BenchmarkListRelaxedvsVolatile {
    
    public enum TestCase {
        CLLElectedUnlink,
        CLLElectedUnlinkVolatile,
        ConcurrentLinkedQueue,
        ConcurrentLinkedQueueRelaxed,
        LRLinkedList,
    }

    private final static int numElements = 1000;            // elements in the array   
    private final int numMilis;  
    private final WorkerThread[] workerThreads;
    private final UserData[] udArray;
    private final CSVDatabase csvdb;
    
    private final CLLElectedUnlink<UserData> clleu;
    private final CLLElectedUnlinkVolatile<UserData> clleuvol;
    private final ConcurrentLinkedQueue<UserData> clq;
    private final ConcurrentLinkedQueueRelaxed<UserData> clqrel;
    //private final LRLinkedList<UserData> lrLinkedList;
    
    private int writePerMil = -1; 
    
    public BenchmarkListRelaxedvsVolatile(int numThreads, int numMilis, CSVDatabase csvdb, int writePerMil) {

        clleu = new CLLElectedUnlink<UserData>();
        clleuvol = new CLLElectedUnlinkVolatile<UserData>();
        clq = new ConcurrentLinkedQueue<UserData>();
        clqrel = new ConcurrentLinkedQueueRelaxed<UserData>();
        //lrLinkedList = new LRLinkedList<UserData>();
        
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
        for (int i = 0; i < numElements; i++) {
            UserData ud = udArray[i];
            clleu.add(ud);
            clleuvol.add(ud);
            clq.add(ud);
            clqrel.add(ud);
            //lrLinkedList.add(ud);
        }

        workerThreads = new WorkerThread[numThreads];
       
        /* Performance tests */
        double writePercentage = writePerMil == 0 ? 0 : writePerMil/10.;
        System.out.println("----- Performance tests numThreads=" +numThreads+"  Writes="+writePercentage+"%  numElements="+numElements+" -----");        
        
        singleTest(numThreads, TestCase.CLLElectedUnlink);
        singleTest(numThreads, TestCase.CLLElectedUnlinkVolatile);
        singleTest(numThreads, TestCase.ConcurrentLinkedQueue);
        singleTest(numThreads, TestCase.ConcurrentLinkedQueueRelaxed);
        //singleTest(numThreads, TestCase.LRLinkedList);
        System.out.println();
    }

    
    public void singleTest(int numThreads, TestCase type) {
        // If we see an error here just increase the number of spaces
        String indentedName = type.toString() + "                                  ".substring(type.toString().length());
        System.out.print("##### "+indentedName+" #####  ");
        
        // Create the threads and then start them all in one go
        for (int i = 0; i < numThreads; i++) {
            workerThreads[i] = new WorkerThread(type, i);
        }
        for (int i = 0; i < numThreads; i++) workerThreads[i].start();

        // Sleep for 10 seconds
        try {
            Thread.sleep(numMilis);
        } catch(InterruptedException e){
            System.out.println("InterruptedException");
        }
        for (int i = 0; i < numThreads; i++) workerThreads[i].quit = true;
               
        try {
            for (int i = 0; i < numThreads; i++) workerThreads[i].join();
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        
        long numAddRemoves = 0;
        long numContains = 0;
        for (int i = 0; i < numThreads; i++) {
            numAddRemoves += workerThreads[i].numAddRemoves;
            numContains += workerThreads[i].numContains;
        }
        final int perSecond = numMilis/1000;
        System.out.println("numAddRemoves/sec = "+(numAddRemoves/perSecond)+"  numContains/sec = "+(numContains/perSecond));

        if (writePerMil == -1) {
            csvdb.addRunDedicated(type.toString(), numThreads-2, 2, numContains/perSecond, numAddRemoves/perSecond);
        } else {
            csvdb.addRun(type.toString(), writePerMil, numThreads, (numContains+numAddRemoves)/perSecond);
        }
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
                    case CLLElectedUnlink:
                        clleu.add(ud);
                        clleu.remove(ud);
                        break;
                    case CLLElectedUnlinkVolatile:
                        clleuvol.add(ud);
                        clleuvol.remove(ud);
                        break;
                    case ConcurrentLinkedQueue:
                        clq.add(ud);
                        clq.remove(ud);
                        break;
                    case ConcurrentLinkedQueueRelaxed:
                        clqrel.add(ud);
                        clqrel.remove(ud);
                        break;
                    //case LRLinkedList:
                    //    lrLinkedList.add(ud);
                    //    lrLinkedList.remove(ud);
                    //    break;
                    }
                    numAddRemoves += 2;
                } else {                   
                    switch (type) {
                    case CLLElectedUnlink:
                        clleu.contains(ud);
                        clleu.contains(ud1);
                        break;
                    case CLLElectedUnlinkVolatile:
                        clleuvol.contains(ud);
                        clleuvol.contains(ud1);
                        break;
                    case ConcurrentLinkedQueue:
                        clq.contains(ud);
                        clq.contains(ud1);
                        break;
                    case ConcurrentLinkedQueueRelaxed:
                        clqrel.contains(ud);
                        clqrel.contains(ud1);
                        break;
                    //case LRLinkedList:
                    //    lrLinkedList.contains(ud);
                    //    lrLinkedList.contains(ud1);
                    //    break;
                    }
                    numContains += 2;
                }
            }
        }
    }
    
    
    /**
     * Mixed task tests, where each thread may do either Writes or Reads
     * @throws InterruptedException 
     */
    static void simpleWritePerMilTest() throws InterruptedException {
        //LinkedList<Integer> threadList = new LinkedList(Arrays.asList(1, 2, 3, 4, 8, 16, 32, 64, 128, 176));
        LinkedList<Integer> threadList = new LinkedList(Arrays.asList(1, 2, 4, 8));
        //LinkedList<Integer> threadList = new LinkedList(Arrays.asList(8));
        
        BenchmarkListRelaxedvsVolatile tests;
        final CSVDatabase csvdb = new CSVDatabase();
        System.out.println("This system has " + Runtime.getRuntime().availableProcessors() + " cores");
        
        for (Integer nThreads : threadList) {
            tests = new BenchmarkListRelaxedvsVolatile(nThreads, 10000, csvdb, 1000);
            tests = null;
            Thread.sleep(1000); // sleep for 1 second to allow the GC to work a bit
        }  // 100%
        
        for (Integer nThreads : threadList) {
            tests = new BenchmarkListRelaxedvsVolatile(nThreads, 10000, csvdb, 100);
            tests = null;
            Thread.sleep(1000); // sleep for 1 second to allow the GC to work a bit
        }  // 10%
        
        for (Integer nThreads : threadList){
            tests = new BenchmarkListRelaxedvsVolatile(nThreads, 10000, csvdb, 10);
            tests = null;
            Thread.sleep(1000); // sleep for 1 second to allow the GC to work a bit
        }   // 1%
        
        for (Integer nThreads : threadList){
            tests = new BenchmarkListRelaxedvsVolatile(nThreads, 10000, csvdb, 1);
            tests = null;
            Thread.sleep(1000); // sleep for 1 second to allow the GC to work a bit
        }    // 0.1%
            
        for (Integer nThreads : threadList){
            tests = new BenchmarkListRelaxedvsVolatile(nThreads, 10000, csvdb, 0);
            tests = null;
            Thread.sleep(1000); // sleep for 1 second to allow the GC to work a bit
        }    // 0.0%
        
        csvdb.saveDB(BenchmarkListRelaxedvsVolatile.numElements);
    }
    
    
    /**
     * @param args
     */
    public static void main(String[] args)  throws InterruptedException {       
        simpleWritePerMilTest();
    }
       
}
