// Declaration slicing is for the retained JVM oracle, never native-body emission.
import java.nio.file.*;
import java.util.*;
import javax.tools.*;
import com.sun.source.tree.*;
import com.sun.source.util.*;
public final class SliceSource {
    public static void main(String[] args)throws Exception {
        String text=Files.readString(Path.of(args[0]));Set<String> roots=new HashSet<>(Arrays.asList(args[2].split(",")));
        var compiler=ToolProvider.getSystemJavaCompiler();
        try(var fm=compiler.getStandardFileManager(null,null,null)){
            var task=(JavacTask)compiler.getTask(null,fm,null,List.of("-proc:none"),null,fm.getJavaFileObjects(args[0]));
            var unit=task.parse().iterator().next();var pos=Trees.instance(task).getSourcePositions();
            ClassTree cls=(ClassTree)unit.getTypeDecls().get(0);Map<String,List<Tree>> members=new LinkedHashMap<>();
            Set<Tree> keep=Collections.newSetFromMap(new IdentityHashMap<>());
            for(Tree m:cls.getMembers()){
                String name=m instanceof MethodTree mt?mt.getName().toString():m instanceof VariableTree v?v.getName().toString():m instanceof ClassTree c?c.getSimpleName().toString():null;
                if(name!=null)members.computeIfAbsent(name,k->new ArrayList<>()).add(m);
                if(m instanceof BlockTree || m instanceof VariableTree v && v.getModifiers().getFlags().contains(javax.lang.model.element.Modifier.STATIC))keep.add(m);
            }
            Set<String> wanted=new HashSet<>(roots);boolean changed;
            do {
                changed=false;
                for(String name:new ArrayList<>(wanted))for(Tree m:members.getOrDefault(name,List.of()))if(keep.add(m))changed=true;
                int count=wanted.size();
                for(Tree m:new ArrayList<>(keep))new TreeScanner<Void,Void>(){
                    public Void visitIdentifier(IdentifierTree n,Void v){wanted.add(n.getName().toString());return super.visitIdentifier(n,v);}
                    public Void visitMemberSelect(MemberSelectTree n,Void v){wanted.add(n.getIdentifier().toString());return super.visitMemberSelect(n,v);}
                }.scan(m,null);
                if(wanted.size()!=count)changed=true;
            }while(changed);
            List<long[]> removals=new ArrayList<>();for(Tree m:cls.getMembers())if(!keep.contains(m))removals.add(new long[]{pos.getStartPosition(unit,m),pos.getEndPosition(unit,m)});
            removals.sort((a,b)->Long.compare(b[0],a[0]));StringBuilder b=new StringBuilder(text);
            for(long[] r:removals)b.replace((int)r[0],(int)r[1],"/* Unreachable declaration excluded from registered source slice. */");
            // Imports are not executable; drop imports whose simple names occur only in excluded declarations.
            String sliced=b.toString();for(ImportTree i:unit.getImports()){
                String name=i.getQualifiedIdentifier().toString();if(name.endsWith(".*"))continue;
                name=name.substring(name.lastIndexOf('.')+1);
                String importText=text.substring((int)pos.getStartPosition(unit,i),(int)pos.getEndPosition(unit,i));
                String without=sliced.replace(importText,"");
                if(!java.util.regex.Pattern.compile("\\b"+java.util.regex.Pattern.quote(name)+"\\b").matcher(without).find())sliced=without;
            }
            Files.writeString(Path.of(args[1]),sliced);
        }
    }
}
