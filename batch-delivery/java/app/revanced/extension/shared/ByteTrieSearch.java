package app.revanced.extension.shared;
import java.nio.charset.StandardCharsets;
// Generated from attributed editable regions, no source-body copying.
final public class ByteTrieSearch extends app.revanced.extension.shared.TrieSearch<byte[]> {
    final private static class ByteTrieNode extends app.revanced.extension.shared.TrieSearch.TrieNode<byte[]> {
         ByteTrieNode() {
            super();
        }
         ByteTrieNode(char nodeCharacterValue) {
            super(nodeCharacterValue);
        }
         app.revanced.extension.shared.TrieSearch.TrieNode<byte[]> createNode(char nodeCharacterValue) {
            return new app.revanced.extension.shared.ByteTrieSearch.ByteTrieNode(nodeCharacterValue);
        }
         char getCharValue(byte[] text, int index) {
            return ((char)text[index]);
        }
         int getTextLength(byte[] text) {
            return text.length;
        }
    }
    public static byte[][] convertStringsToBytes(java.lang.String... strings) {
        final int length = strings.length;
        byte[][] replacement = new byte[length][];
        for (int i = 0; (i < length); i++) {
            replacement[i] = strings[i].getBytes(java.nio.charset.StandardCharsets.UTF_8);
        }
        return replacement;
    }
    public ByteTrieSearch(byte[]... patterns) {
        super(new app.revanced.extension.shared.ByteTrieSearch.ByteTrieNode(), patterns);
    }
}
