package app.revanced.extension.shared.patches.litho;
import androidx.annotation.NonNull;
import app.revanced.extension.shared.ByteTrieSearch;
import app.revanced.extension.shared.Logger;
import app.revanced.extension.shared.settings.BooleanSetting;
// Generated from attributed editable regions, no source-body copying.
abstract public class FilterGroup<T> {
    final public static class FilterGroupResult {
        private app.revanced.extension.shared.settings.BooleanSetting setting;
        private int matchedIndex;
        private int matchedLength;
         FilterGroupResult() {
            this(null, -1, 0);
        }
         FilterGroupResult(app.revanced.extension.shared.settings.BooleanSetting setting, int matchedIndex, int matchedLength) {
            setValues(setting, matchedIndex, matchedLength);
        }
        public void setValues(app.revanced.extension.shared.settings.BooleanSetting setting, int matchedIndex, int matchedLength) {
            this.setting = setting;
            this.matchedIndex = matchedIndex;
            this.matchedLength = matchedLength;
        }
        public app.revanced.extension.shared.settings.BooleanSetting getSetting() {
            return setting;
        }
        public boolean isFiltered() {
            return (matchedIndex >= 0);
        }
        public int getMatchedIndex() {
            return matchedIndex;
        }
        public int getMatchedLength() {
            return matchedLength;
        }
    }
    final protected app.revanced.extension.shared.settings.BooleanSetting setting;
    final public T[] filters;
    public FilterGroup(final app.revanced.extension.shared.settings.BooleanSetting setting, final T... filters) {
        this.setting = setting;
        this.filters = filters;
        if (((filters.length == 0))) {
            throw new java.lang.IllegalArgumentException("Must use one or more filter patterns (zero specified)");
        }
    }
    public boolean isEnabled() {
        return ((setting == null) || setting.get());
    }
    public boolean includeInSearch() {
        return (isEnabled() || (!setting.rebootApp));
    }
    public java.lang.String toString() {
        return ((getClass().getSimpleName() + ": ") + (((setting == null) ? "(null setting)" : setting)));
    }
    abstract public app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult check(final T stack);
    public static class StringFilterGroup extends app.revanced.extension.shared.patches.litho.FilterGroup<java.lang.String> {
        public StringFilterGroup(final app.revanced.extension.shared.settings.BooleanSetting setting, final java.lang.String... filters) {
            super(setting, filters);
        }
        public app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult check(final java.lang.String string) {
            int matchedIndex = -1;
            int matchedLength = 0;
            if ((isEnabled())) {
                for (java.lang.String pattern : filters) {
                    if (((!string.isEmpty()))) {
                        final int indexOf = string.indexOf(pattern);
                        if (((indexOf >= 0))) {
                            matchedIndex = indexOf;
                            matchedLength = pattern.length();
                            break;
                        }
                    }
                }
            }
            return new app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult(setting, matchedIndex, matchedLength);
        }
    }
    public static class ByteArrayFilterGroup extends app.revanced.extension.shared.patches.litho.FilterGroup<byte[]> {
        private volatile int[][] failurePatterns;
        private static int indexOf(final byte[] data, final byte[] pattern, final int[] failure) {
            int patternLength = pattern.length;
            for (int i = 0, j = 0, dataLength = data.length; (i < dataLength); i++) {
                while ((((j > 0) && (pattern[j] != data[i])))) {
                    j = failure[(j - 1)];
                }
                if (((pattern[j] == data[i]))) {
                    j++;
                }
                if (((j == patternLength))) {
                    return ((i - patternLength) + 1);
                }
            }
            return -1;
        }
        private static int[] createFailurePattern(byte[] pattern) {
            final int patternLength = pattern.length;
            final int[] failure = new int[patternLength];
            for (int i = 1, j = 0; (i < patternLength); i++) {
                while ((((j > 0) && (pattern[j] != pattern[i])))) {
                    j = failure[(j - 1)];
                }
                if (((pattern[j] == pattern[i]))) {
                    j++;
                }
                failure[i] = j;
            }
            return failure;
        }
        public ByteArrayFilterGroup(app.revanced.extension.shared.settings.BooleanSetting setting, byte[]... filters) {
            super(setting, filters);
        }
        public ByteArrayFilterGroup(app.revanced.extension.shared.settings.BooleanSetting setting, java.lang.String... filters) {
            super(setting, app.revanced.extension.shared.ByteTrieSearch.convertStringsToBytes(filters));
        }
        private synchronized void buildFailurePatterns() {
            if (((failurePatterns != null))) {
                return;
            }
            app.revanced.extension.shared.Logger.printDebug(() -> ("Building failure array for: " + this));
            int[][] failurePatterns = new int[filters.length][];
            int i = 0;
            for (byte[] pattern : filters) {
                failurePatterns[i++] = app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup.createFailurePattern(pattern);
            }
            this.failurePatterns = failurePatterns;
        }
        public app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult check(final byte[] bytes) {
            int matchedLength = 0;
            int matchedIndex = -1;
            if ((isEnabled())) {
                int[][] failures = failurePatterns;
                if (((failures == null))) {
                    buildFailurePatterns();
                    failures = failurePatterns;
                }
                for (int i = 0, length = filters.length; (i < length); i++) {
                    byte[] filter = filters[i];
                    matchedIndex = app.revanced.extension.shared.patches.litho.FilterGroup.ByteArrayFilterGroup.indexOf(bytes, filter, failures[i]);
                    if (((matchedIndex >= 0))) {
                        matchedLength = filter.length;
                        break;
                    }
                }
            }
            return new app.revanced.extension.shared.patches.litho.FilterGroup.FilterGroupResult(setting, matchedIndex, matchedLength);
        }
    }
}
