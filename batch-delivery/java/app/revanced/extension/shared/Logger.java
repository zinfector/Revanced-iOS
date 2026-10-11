package app.revanced.extension.shared;
public final class Logger {public interface LogMessage{String buildMessageString();}
 public static void printDebug(LogMessage message){if(rv.batch.Platform.debug())rv.batch.Platform.log(message.buildMessageString());}
 public static void printDebug(LogMessage message,Exception error){if(rv.batch.Platform.debug())rv.batch.Platform.log(message.buildMessageString()+" | "+error);}
 public static void printInfo(LogMessage message){rv.batch.Platform.log(message.buildMessageString());}
 public static void printInfo(LogMessage message,Exception error){rv.batch.Platform.log(message.buildMessageString()+" | "+error);}
 public static void printException(LogMessage message){rv.batch.Platform.log(message.buildMessageString());}
 public static void printException(LogMessage message,Exception error){rv.batch.Platform.log(message.buildMessageString()+" | "+error);}}
