package app.revanced.extension.shared;

// Generated from attributed editable regions, no source-body copying.
final public class StringTrieSearch extends app.revanced.extension.shared.TrieSearch<java.lang.String> {
    final private static class StringTrieNode extends app.revanced.extension.shared.TrieSearch.TrieNode<java.lang.String> {
         StringTrieNode() {
            super();
        }
         StringTrieNode(char nodeCharacterValue) {
            super(nodeCharacterValue);
        }
         app.revanced.extension.shared.TrieSearch.TrieNode<java.lang.String> createNode(char nodeValue) {
            return new app.revanced.extension.shared.StringTrieSearch.StringTrieNode(nodeValue);
        }
         char getCharValue(java.lang.String text, int index) {
            return text.charAt(index);
        }
         int getTextLength(java.lang.String text) {
            return text.length();
        }
    }
    public StringTrieSearch(java.lang.String... patterns) {
        super(new app.revanced.extension.shared.StringTrieSearch.StringTrieNode(), patterns);
    }
}
