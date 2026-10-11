"""Operation contracts; none implements a feature's decision algorithm."""
CONTRACTS = {
    'java.lang.Math#min(float,float)': dict(effects=[], family='java.ieee.min'),
    'java.lang.System#currentTimeMillis()': dict(effects=['clock.wall.read'], family='clock.wall_millis', emit_call='rv.translated.PlatformClock.wallMillis'),
    'app.revanced.extension.shared.Logger#printDebug(app.revanced.extension.shared.Logger.LogMessage)': dict(effects=['diagnostic.lazy'], family='diagnostics'),
    'app.revanced.extension.shared.Logger#printException(app.revanced.extension.shared.Logger.LogMessage,java.lang.Exception)': dict(effects=['diagnostic.exception'], family='diagnostics'),
    'app.revanced.extension.shared.Utils#runOnMainThreadDelayed(java.lang.Runnable,long)': dict(effects=['queue.main.enqueue'], family='scheduler', delay_unit='milliseconds', inline=False, callback_errors='log Exception'),
    'app.revanced.extension.shared.Utils#showToastShort(java.lang.String)': dict(effects=['ui.toast'], family='presentation', affinity='main'),
    'app.revanced.extension.shared.StringRef#str(java.lang.String,java.lang.Object[])': dict(effects=['locale.format'], family='localization'),
    'app.revanced.extension.youtube.settings.Settings.BooleanSetting#get()': dict(effects=['preferences.read'], family='preferences.bool'),
    'app.revanced.extension.youtube.settings.Settings.FloatSetting#get()': dict(effects=['preferences.read'], family='preferences.float'),
    'app.revanced.extension.youtube.settings.Settings.FloatSetting#save(float)': dict(effects=['preferences.write'], family='preferences.float'),
    'java.lang.Object#<init>()': dict(effects=['object.allocate'], family='java.object'),
    'java.lang.Runnable#run()': dict(effects=['callback.invoke'], family='java.callback'),
    'java.lang.Thread#<init>(java.lang.Runnable)': dict(effects=['object.allocate'], family='java.thread'),
    'java.lang.Thread#start()': dict(effects=['thread.start'], family='java.thread'),
    'java.lang.Thread#join()': dict(effects=['thread.join'], family='java.thread'),
    'java.lang.IllegalStateException#<init>(java.lang.String)': dict(effects=['object.allocate'], family='java.exception'),
}
