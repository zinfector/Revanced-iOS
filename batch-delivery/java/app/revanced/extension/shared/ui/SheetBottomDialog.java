package app.revanced.extension.shared.ui;
import android.content.*;import android.app.*;import android.view.*;import android.widget.*;
public final class SheetBottomDialog {
 public static class DraggableLinearLayout extends LinearLayout{public DraggableLinearLayout(Context c){super(c);}}
 public static class SlideDialog extends Dialog{public SlideDialog(Context c){super(c);}}
 public static DraggableLinearLayout createMainLayout(Context c,int color){DraggableLinearLayout v=new DraggableLinearLayout(c);v.setOrientation(LinearLayout.VERTICAL);v.setBackgroundColor(color);return v;}
 public static SlideDialog createSlideDialog(Context c,View v,int duration){SlideDialog d=new SlideDialog(c);d.setContentView(v);return d;}}
