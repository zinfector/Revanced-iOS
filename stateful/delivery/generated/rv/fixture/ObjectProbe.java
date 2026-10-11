package rv.fixture;

// Generated from checked editable regions.
final public class ObjectProbe {
    public static int run() {
        rv.fixture.Box box = new rv.fixture.Box(3);
        final int increment = 4;
        java.lang.Runnable callback = () -> {
    box.add(4);
};
        callback.run();
        try {
            throw new java.lang.IllegalStateException("test");
        } catch (java.lang.IllegalStateException ex) {
            box.add(2);
        } finally {
            box.add(1);
        }
        return box.get();
    }
}
