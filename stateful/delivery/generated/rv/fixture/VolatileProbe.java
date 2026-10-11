package rv.fixture;

// Generated from checked editable regions.
final public class VolatileProbe {
    private static volatile boolean ready;
    private static int value;
    public static int run() {
        rv.fixture.VolatileProbe.ready = false;
        rv.fixture.VolatileProbe.value = 0;
        java.lang.Thread writer = new java.lang.Thread(() -> {
    rv.fixture.VolatileProbe.value = 7;
    rv.fixture.VolatileProbe.ready = true;
});
        writer.start();
        try {
            writer.join();
        } catch (java.lang.InterruptedException ex) {
            return -1;
        }
        if ((rv.fixture.VolatileProbe.ready)) {
            return rv.fixture.VolatileProbe.value;
        }
        return -2;
    }
}
