package rv.fixture;

// Generated from checked editable regions.
final public class Box {
    private int value;
    final private java.lang.Object lock = new java.lang.Object();
    public Box(int initial) {
        this.value = initial;
    }
    public int add(int amount) {
        synchronized ((this.lock)) {
            this.value = (this.value + amount);
            return this.value;
        }
    }
    public int get() {
        return this.value;
    }
}
