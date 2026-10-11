import java.nio.file.*;
import java.util.*;
import javax.tools.*;
import com.sun.source.tree.*;
import com.sun.source.util.*;
import com.google.gson.*;
public final class ExportSettings {
    static Object value(ExpressionTree e){
        if(e instanceof LiteralTree l)return Map.of("kind","literal","value",l.getValue());
        if(e instanceof IdentifierTree i)return Map.of("kind","reference","name",i.getName().toString());
        if(e instanceof MemberSelectTree s)return Map.of("kind","member","owner",value(s.getExpression()),"name",s.getIdentifier().toString());
        if(e instanceof UnaryTree u)return Map.of("kind",u.getKind().name(),"value",value(u.getExpression()));
        if(e instanceof BinaryTree b)return Map.of("kind",b.getKind().name(),"left",value(b.getLeftOperand()),"right",value(b.getRightOperand()));
        return Map.of("kind","unsupported","syntax",e.getKind().name());
    }
    public static void main(String[] args)throws Exception {
        var compiler=ToolProvider.getSystemJavaCompiler();var result=new ArrayList<Object>();
        try(var files=compiler.getStandardFileManager(null,null,null)){
            var task=(JavacTask)compiler.getTask(null,files,null,List.of("-encoding","UTF-8","-proc:none"),null,files.getJavaFileObjects(args[0]));
            for(var unit:task.parse())for(var c:unit.getTypeDecls())if(c instanceof ClassTree cls)
                for(var member:cls.getMembers())if(member instanceof VariableTree v && v.getInitializer() instanceof NewClassTree init && v.getType().toString().contains("Setting"))
                    result.add(Map.of("name",v.getName().toString(),"type",v.getType().toString(),"arguments",init.getArguments().stream().limit(3).map(ExportSettings::value).toList()));
        }
        Files.writeString(Path.of(args[1]),new GsonBuilder().setPrettyPrinting().create().toJson(result));
    }
}
