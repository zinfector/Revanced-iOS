package rv.translated;

import java.util.ArrayList;
import java.util.List;

// Deterministic boundary provider, not an app or feature implementation.
public final class Simulator {
    public static long now;
    public static boolean debug;
    public static String failure;
    public static final List<String> trace = new ArrayList<>();
    private static final List<Job> jobs = new ArrayList<>();
    private static final class Job {
        final long due;
        final Runnable callback;
        Job(long due, Runnable callback) { this.due = due; this.callback = callback; }
    }
    public static void reset() { now=1000; debug=false; failure=null; trace.clear(); jobs.clear(); }
    public static void event(String event) {
        trace.add(event);
        if(event.equals(failure)) throw new IllegalStateException("injected");
    }
    public static long wallMillis() { event("clock|"+now); return now; }
    public static void post(Runnable callback,long milliseconds) {
        event("enqueue|"+milliseconds+"|"+now);
        jobs.add(new Job(now+milliseconds,callback));
    }
    public static void runThrough(long time) {
        while(true) {
            int chosen=-1; long due=Long.MAX_VALUE;
            for(int i=0;i<jobs.size();i++) if(jobs.get(i).due<=time && jobs.get(i).due<due) { chosen=i; due=jobs.get(i).due; }
            if(chosen<0) break;
            Job job=jobs.remove(chosen); now=job.due;
            trace.add("callback|"+now); job.callback.run();
        }
        now=time;
    }
    public static int pending() { return jobs.size(); }
    public static String join() { return String.join(";",trace); }
}
