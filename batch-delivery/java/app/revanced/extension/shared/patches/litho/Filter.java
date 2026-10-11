package app.revanced.extension.shared.patches.litho;
import app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup;
import app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
// Generated from attributed editable regions, no source-body copying.
abstract public class Filter {
    public static enum FilterContentType {
        IDENTIFIER(),
PATH(),
ACCESSIBILITY(),
PROTOBUFFER();
    }
    final public java.util.List<app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup> identifierCallbacks = new java.util.ArrayList<>();
    final public java.util.List<app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup> pathCallbacks = new java.util.ArrayList<>();
    final protected void addIdentifierCallbacks(app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup... groups) {
        identifierCallbacks.addAll(java.util.Arrays.asList(groups));
    }
    final protected void addPathCallbacks(app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup... groups) {
        pathCallbacks.addAll(java.util.Arrays.asList(groups));
    }
    public boolean isFiltered(java.lang.String identifier, java.lang.String accessibility, java.lang.String path, byte[] buffer, app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup matchedGroup, app.revanced.extension.shared.patches.litho.Filter.FilterContentType contentType, int contentIndex) {
        return true;
    }
}
