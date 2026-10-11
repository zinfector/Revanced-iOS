package app.revanced.extension.youtube.sponsorblock.ui;
import android.content.Context;import app.revanced.extension.youtube.sponsorblock.objects.SponsorSegment;
public final class SponsorBlockViewController {
 public static void hideAll(){rv.batch.Platform.present("roles.hide_all","sponsorblock");}
 public static void hideNewSegmentLayout(){rv.batch.Platform.present("role.hide","new-segment");}
 public static void showSkipHighlightButton(SponsorSegment segment){rv.batch.Platform.present("role.button.show","sponsor-highlight",segment);}
 public static void hideSkipHighlightButton(){rv.batch.Platform.present("role.button.hide","sponsor-highlight");}
 public static void showSkipSegmentButton(SponsorSegment segment){rv.batch.Platform.present("role.button.show","sponsor-skip",segment);}
 public static void hideSkipSegmentButton(){rv.batch.Platform.present("role.button.hide","sponsor-skip");}
 public static void setSkipSegment(SponsorSegment segment){rv.batch.Platform.present("role.button.value","sponsor-skip",segment);}
 public static Context getOverLaysViewGroupContext(){return (Context)rv.batch.Platform.invoke("context.player_overlay");}}
