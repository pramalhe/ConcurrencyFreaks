package com.concurrencyfreaks.tests;

import java.util.ArrayList;
import java.util.TreeSet;

public class CSVDatabase {
	
	public enum NumOps {
		TotalOps,
		ReadOps,
		WriteOps,
	}

    private final ArrayList<RunEntry> runList = new ArrayList<RunEntry>(); 
    private final TreeSet<String> namesSet = new TreeSet<String>();
    private final TreeSet<String> writesSet = new TreeSet<String>(); 
    private final TreeSet<Integer> threadsSet = new TreeSet<Integer>();
    private final TreeSet<Integer> readersSet = new TreeSet<Integer>();
    private final TreeSet<Integer> writersSet = new TreeSet<Integer>();
    
    private final int numCores = Runtime.getRuntime().availableProcessors();
    
    static public class RunEntry {
        String testName;         // "StampedLock", "ScalableRWLock", etc
        String writePerMil;      // "50% Writes", "20% Writes", etc
        int numThreads;
        int numReaders;
        int numWriters;
        int batchSize;
        // Date?
        long numTotalOpsPerMs;
        long numReadOpsPerMs;
        long numWriteOpsPerMs;
    }
    
    public void addRun(String testName, int writePerMil, int numThreads, long numTotalOpsPerMs) {
        RunEntry run = new RunEntry();
        run.testName = testName;
        //run.writePerMil = ""+(100/(1+writePerMil))+"% Writes";
        run.writePerMil = ""+(writePerMil/10.)+"% Writes";
        run.numThreads = numThreads;
        run.numTotalOpsPerMs = numTotalOpsPerMs;    
        
        writesSet.add(run.writePerMil);
        namesSet.add(run.testName);
        threadsSet.add(run.numThreads);
        runList.add(run);
    }
    
    /**
     * To be called from dedicated thread tests 
     * @param testName
     * @param numThreads
     * @param numReadOpsperMs
     * @param numWriteOpsPerMs
     */
    public void addRunDedicated(String testName, int numReaders, int numWriters, long numReadOpsPerMs, long numWriteOpsPerMs) {
    	RunEntry run = new RunEntry();
    	run.testName = testName;
        run.numThreads = numReaders+numWriters;
        run.numReaders = numReaders;
        run.numWriters = numWriters;
        run.numReadOpsPerMs = numReadOpsPerMs;
        run.numWriteOpsPerMs = numWriteOpsPerMs;
        run.numTotalOpsPerMs = numReadOpsPerMs+numWriteOpsPerMs;    
    	
        namesSet.add(run.testName);
        readersSet.add(numReaders);
        writersSet.add(numWriters);
        runList.add(run);
    }
    
    
    public void saveDB(int numElements) {
        
        System.out.println("numElements="+numElements+",  numCores="+this.numCores);
        
        for (String writePerMil : writesSet) {
            // Display the header
            System.out.print(writePerMil);
            for (String testName : namesSet) System.out.print(", "+testName);
            System.out.println();
                
            for (Integer numThreads : threadsSet) {
                System.out.print(numThreads);
                for (String testName : namesSet) {
                    for (RunEntry run : runList) {    
                        //System.out.println("Comparing "+testName+" with "+run.testName+" and "+numThreads+" with "+run.numThreads);
                        if (!testName.equals(run.testName)) continue;
                        if (numThreads != run.numThreads) continue;
                        if (!writePerMil.equals(run.writePerMil)) continue;
                        System.out.print(", "+run.numTotalOpsPerMs);
                    }
                }
                System.out.println();                
            }
        }
    }
    
    
    /**
     * To be used for dedicated tests
     * 
     */
    public void saveDBDedicated(NumOps opt, int numElements) {
        System.out.println("numElements="+numElements+",  numCores="+this.numCores);
        // Display the header
        System.out.print("Readers, Writers");
        for (String testName : namesSet) System.out.print(", "+testName);
        System.out.println();            
        for (Integer numReaders : readersSet) {
            for (Integer numWriters : writersSet) {
                System.out.print(numReaders+", "+numWriters);
	            for (String testName : namesSet) {
	                for (RunEntry run : runList) {    
	                    //System.out.println("Comparing "+testName+" with "+run.testName+" and "+numThreads+" with "+run.numThreads);
	                    if (!testName.equals(run.testName)) continue;
	                    if (numReaders != run.numReaders) continue;
	                    if (numWriters != run.numWriters) continue;
	                    switch (opt) {
	                    case TotalOps:
	                    	System.out.print(", "+run.numTotalOpsPerMs);
	                    	break;
	                    case ReadOps:
	                    	System.out.print(", "+run.numReadOpsPerMs);
	                    	break;
	                    case WriteOps:
	                    	System.out.print(", "+run.numWriteOpsPerMs);
	                    	break;
	                    }
	                }
	            }
	            System.out.println(); 
            }
        }    	
    }
        
    
}
