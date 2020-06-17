import javax.tools.Diagnostic;
import javax.tools.DiagnosticCollector;
import javax.tools.JavaCompiler;
import javax.tools.JavaCompiler.CompilationTask;
import javax.tools.JavaFileObject;
import javax.tools.SimpleJavaFileObject;
import javax.tools.ToolProvider;
import java.io.File;
import java.io.IOException;
import java.io.PrintWriter;
import java.io.StringWriter;
import java.lang.reflect.InvocationTargetException;
import java.net.URI;
import java.net.URL;
import java.net.URLClassLoader;
import java.util.Arrays;

public class Test {
    public static void main(String[] args) throws Exception {
        JavaCompiler compiler = ToolProvider.getSystemJavaCompiler();
        DiagnosticCollector<JavaFileObject> diagnostics = new DiagnosticCollector<JavaFileObject>();

        String p_message = "filenametest.test";
        String[] splitted_message = p_message.split("\\.");
        String file_name = splitted_message[0].replaceAll("[^A-Za-z0-9 _-]", "");
        file_name = file_name.replaceAll(" ", "_");
        System.out.println(p_message);

        // without this char replacement, I'm getting an error, but this is not tested on Firebase yet.
        p_message = p_message.replaceAll(".", " ");

        StringWriter writer = new StringWriter();
        PrintWriter out = new PrintWriter(writer);
        out.println("public class " + file_name + " {");
        out.println("    public static void main(String[] args) throws Exception {");
        out.println("        throw new Exception(" + p_message + ");");
        out.println("    }");
        out.println("}");
        out.close();
        JavaFileObject file = new JavaSourceFromString(file_name, writer.toString());

        Iterable<? extends JavaFileObject> compilationUnits = Arrays.asList(file);
        CompilationTask task = compiler.getTask(null, null, diagnostics, null, null, compilationUnits);

        boolean success = task.call();
        for (Diagnostic diagnostic : diagnostics.getDiagnostics()) {
            System.out.println(diagnostic.getCode());
            System.out.println(diagnostic.getKind());
            System.out.println(diagnostic.getPosition());
            System.out.println(diagnostic.getStartPosition());
            System.out.println(diagnostic.getEndPosition());
            System.out.println(diagnostic.getSource());
            System.out.println(diagnostic.getMessage(null));

        }
        System.out.println("Success: " + success);
        
        if (success) {
            try {
                URLClassLoader classLoader = URLClassLoader.newInstance(new URL[] { new File("").toURI().toURL() });
                Class.forName(file_name, true, classLoader).getDeclaredMethod("main", new Class[] { String[].class }).invoke(null, new Object[] { null });
            } catch (Exception e) {
                throw new Exception(e);
            }
        }
    }
}

class JavaSourceFromString extends SimpleJavaFileObject {
    final String code;

    JavaSourceFromString(String name, String code) {
        super(URI.create("string:///" + name.replace('.','/') + Kind.SOURCE.extension),Kind.SOURCE);
        this.code = code;
    }

    @Override
    public CharSequence getCharContent(boolean ignoreEncodingErrors) {
        return code;
    }
}
