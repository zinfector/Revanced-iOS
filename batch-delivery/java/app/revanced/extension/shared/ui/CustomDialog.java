package app.revanced.extension.shared.ui;
import android.app.*;import android.content.*;import android.widget.*;import android.util.Pair;
public final class CustomDialog {public static Pair<Dialog,LinearLayout> create(Context c,CharSequence title,CharSequence message,EditText edit,String ok,Runnable yes,Runnable no,String neutral,Runnable extra,boolean dismiss){
 LinearLayout layout=new LinearLayout(c);Dialog dialog=new Dialog(c);dialog.setContentView(layout);rv.batch.Platform.present("dialog.configure",dialog,title,message,edit,ok,yes,no,neutral,extra,dismiss);return Pair.create(dialog,layout);}}
