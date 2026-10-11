// Attributed, typed export. No source text is emitted as executable code.
import java.nio.file.*;
import java.util.*;
import javax.tools.*;
import javax.lang.model.element.*;
import javax.lang.model.type.*;
import javax.lang.model.util.*;
import com.sun.source.tree.*;
import com.sun.source.util.*;
import com.google.gson.*;

public final class ExportPatch {
    static Trees trees;
    static Types types;
    static CompilationUnitTree unit;
    static SourcePositions positions;
    static String filename;
    static final Map<Element,String> localIds = new IdentityHashMap<>();

    static TreePath path(Tree t) { return TreePath.getPath(unit,t); }
    static Element element(Tree t) { return trees.getElement(path(t)); }
    static String type(Tree t) {
        TypeMirror tm=trees.getTypeMirror(path(t));
        if(tm==null) return "void";
        if(tm.getKind()==TypeKind.ERROR) throw new IllegalStateException("Unresolved type: "+t);
        return tm.toString();
    }
    static String owner(Element e) { return ((TypeElement)e.getEnclosingElement()).getQualifiedName().toString(); }
    static String callable(ExecutableElement e) {
        return owner(e)+"#"+e.getSimpleName()+"("+String.join(",",e.getParameters().stream().map(p->types.erasure(p.asType()).toString()).toList())+")";
    }
    static String variable(VariableElement e) {
        if(e.getKind().isField()) return owner(e)+"#"+e.getSimpleName();
        return localIds.computeIfAbsent(e,k->{
            var decl=trees.getPath(e); long start=decl==null?-1:positions.getStartPosition(unit,decl.getLeaf());
            return "local_"+start+"_"+e.getSimpleName();
        });
    }
    static Map<String,Object> row(Tree t) {
        var m=new LinkedHashMap<String,Object>();
        long start=positions.getStartPosition(unit,t),end=positions.getEndPosition(unit,t);
        m.put("kind",t.getKind().name());m.put("id","n_"+start+"_"+end+"_"+t.getKind().name());
        m.put("origin",Map.of("source",filename,"start",start,"end",end,"line",start<0?-1:unit.getLineMap().getLineNumber(start)));
        return m;
    }
    static Object node(Tree t) {
        if(t==null) return null;
        var m=row(t);
        if(t instanceof ExpressionTree || t instanceof VariableTree) m.put("type",type(t));
        if(t instanceof BlockTree b) m.put("statements",b.getStatements().stream().filter(x->positions.getEndPosition(unit,x)>=0).map(ExportPatch::node).toList());
        else if(t instanceof VariableTree v) {
            var e=(VariableElement)element(v);m.put("symbol",variable(e));m.put("name",v.getName().toString());m.put("declared_type",e.asType().toString());
            m.put("modifiers",e.getModifiers().stream().map(Object::toString).sorted().toList());m.put("value",node(v.getInitializer()));
        } else if(t instanceof ReturnTree r) m.put("value",node(r.getExpression()));
        else if(t instanceof IfTree i) {m.put("condition",node(i.getCondition()));m.put("then",node(i.getThenStatement()));m.put("else",node(i.getElseStatement()));}
        else if(t instanceof LiteralTree l) {
            Object value=l.getValue();m.put("value",value instanceof Long?value.toString():value);
        } else if(t instanceof IdentifierTree || t instanceof MemberSelectTree) {
            Element e=element(t);
            if(t instanceof IdentifierTree i && i.getName().contentEquals("this")) m.put("this",true);
            else if(e instanceof VariableElement v) {
                m.put("symbol",variable(v));m.put("name",v.getSimpleName().toString());m.put("field",v.getKind().isField());
                m.put("modifiers",v.getModifiers().stream().map(Object::toString).sorted().toList());
                if(v.getKind().isField()) m.put("owner",owner(v));
                if(t instanceof MemberSelectTree s && !v.getModifiers().contains(Modifier.STATIC)) m.put("receiver",node(s.getExpression()));
                Object constant=v.getConstantValue();if(constant!=null)m.put("constant",constant instanceof Long?constant.toString():constant);
            } else if(e instanceof TypeElement cls) m.put("class",cls.getQualifiedName().toString());
            else if(t.toString().equals("this")) m.put("this",true);
            else m.put("unsupported",true);
        } else if(t instanceof MethodInvocationTree call) {
            var e=(ExecutableElement)element(t);m.put("target",callable(e));m.put("owner",owner(e));m.put("name",e.getSimpleName().toString());
            m.put("static",e.getModifiers().contains(Modifier.STATIC));
            m.put("parameter_types",e.getParameters().stream().map(p->types.erasure(p.asType()).toString()).toList());
            if(!e.getModifiers().contains(Modifier.STATIC) && call.getMethodSelect() instanceof MemberSelectTree s)m.put("receiver",node(s.getExpression()));
            m.put("arguments",call.getArguments().stream().map(ExportPatch::node).toList());
        } else if(t instanceof BinaryTree b){m.put("left",node(b.getLeftOperand()));m.put("right",node(b.getRightOperand()));}
        else if(t instanceof UnaryTree u)m.put("value",node(u.getExpression()));
        else if(t instanceof ParenthesizedTree p)m.put("value",node(p.getExpression()));
        else if(t instanceof ExpressionStatementTree e)m.put("value",node(e.getExpression()));
        else if(t instanceof AssignmentTree a){m.put("target",node(a.getVariable()));m.put("value",node(a.getExpression()));}
        else if(t instanceof ConditionalExpressionTree c){m.put("condition",node(c.getCondition()));m.put("then",node(c.getTrueExpression()));m.put("else",node(c.getFalseExpression()));}
        else if(t instanceof LambdaExpressionTree l) {
            m.put("parameters",l.getParameters().stream().map(ExportPatch::node).toList());m.put("body",node(l.getBody()));
            long begin=positions.getStartPosition(unit,l),end=positions.getEndPosition(unit,l);
            var captures=new LinkedHashMap<String,Object>();
            new TreeScanner<Void,Void>() {
                @Override public Void visitIdentifier(IdentifierTree i,Void unused) {
                    var e=element(i);
                    if(e instanceof VariableElement v && !v.getKind().isField() && v.getConstantValue()==null) {
                        var decl=trees.getPath(e);long at=decl==null?-1:positions.getStartPosition(unit,decl.getLeaf());
                        if(at<begin || at>end) captures.put(variable(v),Map.of("symbol",variable(v),"name",v.getSimpleName().toString(),"type",v.asType().toString()));
                    }
                    return super.visitIdentifier(i,unused);
                }
            }.scan(l.getBody(),null);
            m.put("captures",new ArrayList<>(captures.values()));
        } else if(t instanceof TryTree tr) {
            if(!tr.getResources().isEmpty())m.put("unsupported",true);
            m.put("body",node(tr.getBlock()));m.put("catches",tr.getCatches().stream().map(ExportPatch::node).toList());m.put("finally",node(tr.getFinallyBlock()));
        } else if(t instanceof CatchTree c){m.put("parameter",node(c.getParameter()));m.put("body",node(c.getBlock()));}
        else if(t instanceof SynchronizedTree s){m.put("lock",node(s.getExpression()));m.put("body",node(s.getBlock()));}
        else if(t instanceof NewClassTree n) {
            var e=(ExecutableElement)element(t);m.put("target",callable(e));m.put("class",owner(e));m.put("arguments",n.getArguments().stream().map(ExportPatch::node).toList());
            if(n.getClassBody()!=null || n.getEnclosingExpression()!=null)m.put("unsupported",true);
        } else if(t instanceof ThrowTree thr)m.put("value",node(thr.getExpression()));
        else if(t instanceof TypeCastTree c){m.put("cast_type",type(c));m.put("value",node(c.getExpression()));}
        else if(t instanceof WhileLoopTree w){m.put("condition",node(w.getCondition()));m.put("body",node(w.getStatement()));}
        else m.put("unsupported",true);
        return m;
    }
    static Map<String,Object> cls(ClassTree c) {
        if(c.getExtendsClause()!=null || !c.getImplementsClause().isEmpty() || !c.getTypeParameters().isEmpty())
            throw new IllegalStateException("Inheritance/generic class frontier is not yet admitted");
        var e=(TypeElement)element(c);var m=new LinkedHashMap<String,Object>();m.put("name",e.getQualifiedName().toString());
        m.put("simple_name",c.getSimpleName().toString());m.put("modifiers",e.getModifiers().stream().map(Object::toString).sorted().toList());
        var fields=new ArrayList<Object>();var methods=new ArrayList<Object>();
        for(Tree member:c.getMembers()) {
            if(positions.getEndPosition(unit,member)<0)continue;
            if(member instanceof VariableTree) fields.add(node(member));
            else if(member instanceof MethodTree mt && mt.getBody()!=null) {
                if(!mt.getThrows().isEmpty() || !mt.getTypeParameters().isEmpty())throw new IllegalStateException("Throws/generic method frontier is not yet admitted");
                var me=(ExecutableElement)element(mt);var method=row(mt);
                method.put("symbol",callable(me));method.put("name",mt.getName().toString());
                method.put("result",me.getReturnType().toString());method.put("constructor",mt.getReturnType()==null);
                method.put("modifiers",me.getModifiers().stream().map(Object::toString).sorted().toList());
                method.put("parameters",mt.getParameters().stream().map(ExportPatch::node).toList());method.put("body",node(mt.getBody()));
                methods.add(method);
            } else throw new IllegalStateException("Unsupported class member: "+member.getKind());
        }
        m.put("fields",fields);m.put("methods",methods);return m;
    }
    public static void main(String[] args) throws Exception {
        var compiler=ToolProvider.getSystemJavaCompiler();var diagnostics=new DiagnosticCollector<JavaFileObject>();
        var result=new ArrayList<Object>();
        try(var files=compiler.getStandardFileManager(diagnostics,null,null)) {
            var sources=files.getJavaFileObjectsFromStrings(Arrays.asList(args).subList(3,args.length));
            var task=(JavacTask)compiler.getTask(null,files,diagnostics,List.of("-proc:none","-classpath",args[1],"-sourcepath",args[2]),null,sources);
            var units=new ArrayList<CompilationUnitTree>();task.parse().forEach(units::add);task.analyze();
            for(var d:diagnostics.getDiagnostics())if(d.getKind()==Diagnostic.Kind.ERROR)throw new IllegalStateException(d.toString());
            trees=Trees.instance(task);types=task.getTypes();positions=trees.getSourcePositions();
            for(var u:units) {
                unit=u;filename=Paths.get(u.getSourceFile().toUri()).toString();
                for(var declaration:u.getTypeDecls()) {
                    if(!(declaration instanceof ClassTree c))throw new IllegalStateException("Class required");
                    result.add(cls(c));
                }
            }
        }
        Files.writeString(Paths.get(args[0]),new GsonBuilder().serializeNulls().setPrettyPrinting().create().toJson(Map.of("schema","editable-code.patch.regions.v2","classes",result,"types_attributed",true)));
    }
}
