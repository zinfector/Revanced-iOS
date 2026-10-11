package app.revanced.extension.shared;
import androidx.annotation.Nullable;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Objects;
// Generated from attributed editable regions, no source-body copying.
abstract public class TrieSearch<T> {
    abstract public static interface TriePatternMatchedCallback<T> {
        abstract public boolean patternMatched(T textSearched, int matchedStartIndex, int matchedLength, java.lang.Object callbackParameter);
    }
    final private static class TrieCompressedPath<T> {
        final T pattern;
        final int patternStartIndex;
        final int patternLength;
        final app.revanced.extension.shared.TrieSearch.TriePatternMatchedCallback<T> callback;
         TrieCompressedPath(T pattern, int patternStartIndex, int patternLength, app.revanced.extension.shared.TrieSearch.TriePatternMatchedCallback<T> callback) {
            this.pattern = pattern;
            this.patternStartIndex = patternStartIndex;
            this.patternLength = patternLength;
            this.callback = callback;
        }
         boolean matches(app.revanced.extension.shared.TrieSearch.TrieNode<T> enclosingNode, T searchText, int searchTextLength, int searchTextIndex, java.lang.Object callbackParameter) {
            if ((((searchTextLength - searchTextIndex) < (patternLength - patternStartIndex)))) {
                return false;
            }
            for (int i = searchTextIndex, j = patternStartIndex; (j < patternLength); i++, j++) {
                if (((enclosingNode.getCharValue(searchText, i) != enclosingNode.getCharValue(pattern, j)))) {
                    return false;
                }
            }
            return ((callback == null) || callback.patternMatched(searchText, (searchTextIndex - patternStartIndex), patternLength, callbackParameter));
        }
    }
    abstract static class TrieNode<T> {
        final private static char ROOT_NODE_CHARACTER_VALUE = 0;
        final private static int CHILDREN_ARRAY_INCREASE_SIZE_INCREMENT = 2;
        final private char nodeValue;
        private app.revanced.extension.shared.TrieSearch.TrieCompressedPath<T> leaf;
        private app.revanced.extension.shared.TrieSearch.TrieNode<T>[] children;
        private java.util.List<app.revanced.extension.shared.TrieSearch.TriePatternMatchedCallback<T>> endOfPatternCallback;
         TrieNode() {
            this.nodeValue = ROOT_NODE_CHARACTER_VALUE;
        }
         TrieNode(char nodeCharacterValue) {
            this.nodeValue = nodeCharacterValue;
        }
        private void addPattern(T pattern, int patternIndex, int patternLength, app.revanced.extension.shared.TrieSearch.TriePatternMatchedCallback<T> callback) {
            if (((patternIndex == patternLength))) {
                if (((endOfPatternCallback == null))) {
                    endOfPatternCallback = new java.util.ArrayList<>(1);
                }
                endOfPatternCallback.add(callback);
                return;
            }
            if (((leaf != null))) {
                if (((children != null))) {
                    throw new java.lang.IllegalStateException();
                }
                children = new app.revanced.extension.shared.TrieSearch.TrieNode[1];
                app.revanced.extension.shared.TrieSearch.TrieCompressedPath<T> temp = leaf;
                leaf = null;
                addPattern(temp.pattern, temp.patternStartIndex, temp.patternLength, temp.callback);
            } else {
                if (((children == null))) {
                    leaf = new app.revanced.extension.shared.TrieSearch.TrieCompressedPath<>(pattern, patternIndex, patternLength, callback);
                    return;
                }
            }
            final char character = getCharValue(pattern, patternIndex);
            final int arrayIndex = app.revanced.extension.shared.TrieSearch.TrieNode.hashIndexForTableSize(children.length, character);
            app.revanced.extension.shared.TrieSearch.TrieNode<T> child = children[arrayIndex];
            if (((child == null))) {
                child = createNode(character);
                children[arrayIndex] = child;
            } else {
                if (((child.nodeValue != character))) {
                    child = createNode(character);
                    expandChildArray(child);
                }
            }
            child.addPattern(pattern, (patternIndex + 1), patternLength, callback);
        }
        private void expandChildArray(app.revanced.extension.shared.TrieSearch.TrieNode<T> child) {
            int replacementArraySize = java.util.Objects.requireNonNull(children).length;
            while ((true)) {
                replacementArraySize += CHILDREN_ARRAY_INCREASE_SIZE_INCREMENT;
                app.revanced.extension.shared.TrieSearch.TrieNode<T>[] replacement = new app.revanced.extension.shared.TrieSearch.TrieNode[replacementArraySize];
                app.revanced.extension.shared.TrieSearch.TrieNode.addNodeToArray(replacement, child);
                boolean collision = false;
                for (app.revanced.extension.shared.TrieSearch.TrieNode<T> existingChild : children) {
                    if (((existingChild != null))) {
                        if (((!app.revanced.extension.shared.TrieSearch.TrieNode.addNodeToArray(replacement, existingChild)))) {
                            collision = true;
                            break;
                        }
                    }
                }
                if ((collision)) {
                    continue;
                }
                children = replacement;
                return;
            }
        }
        private static <T> boolean addNodeToArray(app.revanced.extension.shared.TrieSearch.TrieNode<T>[] array, app.revanced.extension.shared.TrieSearch.TrieNode<T> childToAdd) {
            final int insertIndex = app.revanced.extension.shared.TrieSearch.TrieNode.hashIndexForTableSize(array.length, childToAdd.nodeValue);
            if (((array[insertIndex] != null))) {
                return false;
            }
            array[insertIndex] = childToAdd;
            return true;
        }
        private static int hashIndexForTableSize(int arraySize, char nodeValue) {
            return (nodeValue % arraySize);
        }
        private static <T> boolean matches(final app.revanced.extension.shared.TrieSearch.TrieNode<T> startNode, final T searchText, int searchTextIndex, final int searchTextEndIndex, final java.lang.Object callbackParameter) {
            app.revanced.extension.shared.TrieSearch.TrieNode<T> node = startNode;
            int currentMatchLength = 0;
            while ((true)) {
                app.revanced.extension.shared.TrieSearch.TrieCompressedPath<T> leaf = node.leaf;
                if ((((leaf != null) && leaf.matches(startNode, searchText, searchTextEndIndex, searchTextIndex, callbackParameter)))) {
                    return true;
                }
                java.util.List<app.revanced.extension.shared.TrieSearch.TriePatternMatchedCallback<T>> endOfPatternCallback = node.endOfPatternCallback;
                if (((endOfPatternCallback != null))) {
                    final int matchStartIndex = (searchTextIndex - currentMatchLength);
                    for (app.revanced.extension.shared.TrieSearch.TriePatternMatchedCallback<T> callback : endOfPatternCallback) {
                        if (((callback == null))) {
                            return true;
                        }
                        if ((callback.patternMatched(searchText, matchStartIndex, currentMatchLength, callbackParameter))) {
                            return true;
                        }
                    }
                }
                app.revanced.extension.shared.TrieSearch.TrieNode<T>[] children = node.children;
                if (((children == null))) {
                    return false;
                }
                if (((searchTextIndex == searchTextEndIndex))) {
                    return false;
                }
                final char character = startNode.getCharValue(searchText, searchTextIndex);
                final int arrayIndex = app.revanced.extension.shared.TrieSearch.TrieNode.hashIndexForTableSize(children.length, character);
                app.revanced.extension.shared.TrieSearch.TrieNode<T> child = children[arrayIndex];
                if ((((child == null) || (child.nodeValue != character)))) {
                    return false;
                }
                node = child;
                searchTextIndex++;
                currentMatchLength++;
            }
        }
        private int estimatedNumberOfPointersUsed() {
            int numberOfPointers = 4;
            if (((leaf != null))) {
                numberOfPointers += 4;
            }
            if (((endOfPatternCallback != null))) {
                numberOfPointers += endOfPatternCallback.size();
            }
            if (((children != null))) {
                numberOfPointers += children.length;
                for (app.revanced.extension.shared.TrieSearch.TrieNode<T> child : children) {
                    if (((child != null))) {
                        numberOfPointers += child.estimatedNumberOfPointersUsed();
                    }
                }
            }
            return numberOfPointers;
        }
        abstract app.revanced.extension.shared.TrieSearch.TrieNode<T> createNode(char nodeValue);
        abstract char getCharValue(T text, int index);
        abstract int getTextLength(T text);
    }
    final private app.revanced.extension.shared.TrieSearch.TrieNode<T> root;
    final private java.util.List<T> patterns = new java.util.ArrayList<>();
     TrieSearch(app.revanced.extension.shared.TrieSearch.TrieNode<T> root, T... patterns) {
        this.root = java.util.Objects.requireNonNull(root);
        addPatterns(patterns);
    }
    final public void addPatterns(T... patterns) {
        for (T pattern : patterns) {
            addPattern(pattern);
        }
    }
    public void addPattern(T pattern) {
        addPattern(pattern, root.getTextLength(pattern), null);
    }
    public void addPattern(T pattern, app.revanced.extension.shared.TrieSearch.TriePatternMatchedCallback<T> callback) {
        addPattern(pattern, root.getTextLength(pattern), java.util.Objects.requireNonNull(callback));
    }
     void addPattern(T pattern, int patternLength, app.revanced.extension.shared.TrieSearch.TriePatternMatchedCallback<T> callback) {
        if (((patternLength == 0))) {
            return;
        }
        patterns.add(pattern);
        root.addPattern(pattern, 0, patternLength, callback);
    }
    final public boolean matches(T textToSearch) {
        return matches(textToSearch, 0);
    }
    public boolean matches(T textToSearch, java.lang.Object callbackParameter) {
        return matches(textToSearch, 0, root.getTextLength(textToSearch), java.util.Objects.requireNonNull(callbackParameter));
    }
    public boolean matches(T textToSearch, int startIndex) {
        return matches(textToSearch, startIndex, root.getTextLength(textToSearch));
    }
    final public boolean matches(T textToSearch, int startIndex, int endIndex) {
        return matches(textToSearch, startIndex, endIndex, null);
    }
    public boolean matches(T textToSearch, int startIndex, int endIndex, java.lang.Object callbackParameter) {
        return matches(textToSearch, root.getTextLength(textToSearch), startIndex, endIndex, callbackParameter);
    }
    private boolean matches(T textToSearch, int textToSearchLength, int startIndex, int endIndex, java.lang.Object callbackParameter) {
        if (((endIndex > textToSearchLength))) {
            throw new java.lang.IllegalArgumentException(((("endIndex: " + endIndex) + " is greater than texToSearchLength: ") + textToSearchLength));
        }
        if ((patterns.isEmpty())) {
            return false;
        }
        for (int i = startIndex; (i < endIndex); i++) {
            if ((app.revanced.extension.shared.TrieSearch.TrieNode.matches(root, textToSearch, i, endIndex, callbackParameter))) {
                return true;
            }
        }
        return false;
    }
    public int getEstimatedMemorySize() {
        if ((patterns.isEmpty())) {
            return 0;
        }
        final int numberOfBytesPerPointer = 4;
        return ((int)java.lang.Math.ceil((((numberOfBytesPerPointer * root.estimatedNumberOfPointersUsed())) / 1024.0d)));
    }
    public int numberOfPatterns() {
        return patterns.size();
    }
    public java.util.List<T> getPatterns() {
        return java.util.Collections.unmodifiableList(patterns);
    }
}
