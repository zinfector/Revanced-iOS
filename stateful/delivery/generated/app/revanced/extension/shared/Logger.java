package app.revanced.extension.shared;
import rv.translated.Simulator;
public final class Logger {
    public interface LogMessage {String buildMessageString();}
    public static void printDebug(LogMessage message){if(Simulator.debug)Simulator.event("debug|"+message.buildMessageString());}
    public static void printException(LogMessage message,Exception ex){Simulator.trace.add("error|"+message.buildMessageString()+"|"+ex.getClass().getSimpleName());}
}
