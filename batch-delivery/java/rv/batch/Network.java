package rv.batch;
import java.net.*;
import java.io.*;
import java.util.*;

/** Transport only. Route construction, retries, response parsing and voting stay in source. */
public final class Network {
 public static URLConnection open(URL url){return new Connection(url);}
 public static final class Response {
  public final int status;public final byte[] body;public final Map<String,List<String>> headers;
  public Response(int s,byte[] b,Map<String,List<String>> h){status=s;body=b;headers=h;}
 }
 public static final class Request {
  public final String url,method;public final Map<String,List<String>> headers;public final byte[] body;
  public final int connectTimeout,readTimeout;public final boolean followRedirects,useCaches;
  Request(Connection c){url=c.getURL().toString();method=c.getRequestMethod();headers=new LinkedHashMap<>(c.headers);body=c.output.toByteArray();connectTimeout=c.getConnectTimeout();readTimeout=c.getReadTimeout();followRedirects=c.getInstanceFollowRedirects();useCaches=c.getUseCaches();}
 }
 private static final class Connection extends HttpURLConnection {
  final Map<String,List<String>> headers=new LinkedHashMap<>();final ByteArrayOutputStream output=new ByteArrayOutputStream();Response response;
  Connection(URL u){super(u);}
  public synchronized void connect()throws IOException {if(response!=null)return;Object v=Platform.invoke("http.request",new Request(this));if(!(v instanceof Response))throw new IOException("HTTP adapter returned no response");response=(Response)v;connected=true;responseCode=response.status;}
  public void disconnect(){connected=false;Platform.invoke("http.disconnect",this);}public boolean usingProxy(){return false;}
  public void setRequestProperty(String k,String v){if(connected)throw new IllegalStateException();headers.put(k,new ArrayList<>(Collections.singletonList(v)));}
  public void addRequestProperty(String k,String v){if(connected)throw new IllegalStateException();headers.computeIfAbsent(k,x->new ArrayList<>()).add(v);}
  public String getRequestProperty(String k){List<String> v=headers.get(k);return v==null||v.isEmpty()?null:v.get(0);}public Map<String,List<String>> getRequestProperties(){if(connected)throw new IllegalStateException();return Collections.unmodifiableMap(headers);}
  public OutputStream getOutputStream()throws IOException {if(!getDoOutput())throw new ProtocolException("Output disabled");if(connected)throw new ProtocolException("Already connected");return output;}
  public int getResponseCode()throws IOException {connect();return response.status;}
  public InputStream getInputStream()throws IOException {connect();if(response.status>=400)throw new IOException("HTTP "+response.status);return new ByteArrayInputStream(response.body);}
  public InputStream getErrorStream(){if(response==null||response.status<400)return null;return new ByteArrayInputStream(response.body);}
  public Map<String,List<String>> getHeaderFields(){try{connect();return response.headers;}catch(IOException e){return Collections.emptyMap();}}
 }
}
