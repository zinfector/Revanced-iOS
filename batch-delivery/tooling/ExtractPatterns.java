import java.nio.file.*;
import com.google.gson.*;
public final class ExtractPatterns {
 public static void main(String[] args)throws Exception {
  java.util.regex.Pattern p=(java.util.regex.Pattern)Class.forName("android.util.Patterns").getField("WEB_URL").get(null);
  Files.writeString(Path.of(args[0]),new Gson().toJson(p.pattern()));
 }
}
