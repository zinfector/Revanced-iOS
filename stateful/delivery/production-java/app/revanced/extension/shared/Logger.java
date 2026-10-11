package app.revanced.extension.shared;
import rv.platform.NativeBridge;
public final class Logger {
    public interface LogMessage {String buildMessageString();}
    public static void printDebug(LogMessage message){if(NativeBridge.debugEnabled())NativeBridge.log(message.buildMessageString());}
    public static void printException(LogMessage message,Exception ex){NativeBridge.log(message.buildMessageString()+" | "+ex);}
}
