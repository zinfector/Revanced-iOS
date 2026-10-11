package app.revanced.extension.youtube.shared;
public final class NavigationBar {public enum NavigationButton {HOME,SHORTS,EXPLORE,CREATE,SUBSCRIPTIONS,LIBRARY,YOU;
 public static NavigationButton getSelected(){return getSelectedNavigationButton();}public static NavigationButton getSelectedNavigationButton(){return NavigationButton.valueOf((String)rv.batch.Platform.read("navigation.selected","HOME"));}}public static boolean isSearchBarActive(){return (Boolean)rv.batch.Platform.read("navigation.search_active",false);}}
