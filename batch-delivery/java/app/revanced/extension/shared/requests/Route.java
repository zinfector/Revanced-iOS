package app.revanced.extension.shared.requests;

// Generated from attributed editable regions, no source-body copying.
public class Route {
    final private java.lang.String route;
    final private app.revanced.extension.shared.requests.Route.Method method;
    final private int paramCount;
    public Route(app.revanced.extension.shared.requests.Route.Method method, java.lang.String route) {
        this.method = method;
        this.route = route;
        this.paramCount = countMatches(route, '{');
        if (((paramCount != countMatches(route, '}')))) {
            throw new java.lang.IllegalArgumentException("Not enough parameters");
        }
    }
    public app.revanced.extension.shared.requests.Route.Method getMethod() {
        return method;
    }
    public app.revanced.extension.shared.requests.Route.CompiledRoute compile(java.lang.String... params) {
        if (((params.length != paramCount))) {
            throw new java.lang.IllegalArgumentException(((((("Error compiling route [" + route) + "], incorrect amount of parameters provided. Expected: ") + paramCount) + ", provided: ") + params.length));
        }
        java.lang.StringBuilder compiledRoute = new java.lang.StringBuilder(route);
        for (int i = 0; (i < paramCount); i++) {
            int paramStart = compiledRoute.indexOf("{");
            int paramEnd = compiledRoute.indexOf("}");
            compiledRoute.replace(paramStart, (paramEnd + 1), params[i]);
        }
        return new app.revanced.extension.shared.requests.Route.CompiledRoute(this, compiledRoute.toString());
    }
    public static class CompiledRoute {
        final private app.revanced.extension.shared.requests.Route baseRoute;
        final private java.lang.String compiledRoute;
        private CompiledRoute(app.revanced.extension.shared.requests.Route baseRoute, java.lang.String compiledRoute) {
            this.baseRoute = baseRoute;
            this.compiledRoute = compiledRoute;
        }
        public java.lang.String getCompiledRoute() {
            return compiledRoute;
        }
        public app.revanced.extension.shared.requests.Route.Method getMethod() {
            return baseRoute.method;
        }
    }
    private int countMatches(java.lang.CharSequence seq, char c) {
        int count = 0;
        for (int i = 0, length = seq.length(); (i < length); i++) {
            if (((seq.charAt(i) == c))) {
                count++;
            }
        }
        return count;
    }
    public static enum Method {
        GET(),
POST();
    }
}
