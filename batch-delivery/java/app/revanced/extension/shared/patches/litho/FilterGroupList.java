package app.revanced.extension.shared.patches.litho;
import androidx.annotation.NonNull;
import app.revanced.extension.shared.ByteTrieSearch;
import app.revanced.extension.shared.StringTrieSearch;
import app.revanced.extension.shared.TrieSearch;
import app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup;
import app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Iterator;
import java.util.List;
// Generated from attributed editable regions, no source-body copying.
abstract public class FilterGroupList<V, T extends app.revanced.extension.shared.patches.litho.FilterGroup<V>> implements java.lang.Iterable<T> {
    final private java.util.List<T> filterGroups = new java.util.ArrayList<>();
    final private app.revanced.extension.shared.TrieSearch<V> search = createSearchGraph();
    final public void addAll(final T... groups) {
        filterGroups.addAll(java.util.Arrays.asList(groups));
        for (T group : groups) {
            if (((!group.includeInSearch()))) {
                continue;
            }
            for (V pattern : group.filters) {
                search.addPattern(pattern, (V textSearched, int matchedStartIndex, int matchedLength, java.lang.Object callbackParameter) -> {
    if ((group.isEnabled())) {
        app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult result = ((app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult)callbackParameter);
        result.setValues(group.setting, matchedStartIndex, matchedLength);
        return true;
    }
    return false;
});
            }
        }
    }
    public java.util.Iterator<T> iterator() {
        return filterGroups.iterator();
    }
    public app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult check(V stack) {
        app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult result = new app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult();
        search.matches(stack, result);
        return result;
    }
    abstract protected app.revanced.extension.shared.TrieSearch<V> createSearchGraph();
    final public static class StringFilterGroupList extends app.revanced.extension.shared.patches.litho.FilterGroupList<java.lang.String, app.revanced.extension.shared.patches.litho.FilterGroup.StringFilterGroup> {
        protected app.revanced.extension.shared.StringTrieSearch createSearchGraph() {
            return new app.revanced.extension.shared.StringTrieSearch();
        }
    }
    final public static class ByteArrayFilterGroupList extends app.revanced.extension.shared.patches.litho.FilterGroupList<byte[], app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup> {
        protected app.revanced.extension.shared.ByteTrieSearch createSearchGraph() {
            return new app.revanced.extension.shared.ByteTrieSearch();
        }
    }
}
