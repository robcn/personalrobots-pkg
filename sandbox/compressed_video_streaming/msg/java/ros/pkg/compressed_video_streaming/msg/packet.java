/* auto-generated by genmsg_cpp from /u/ethand/rosPostCommonMsgs/ros-pkg/sandbox/compressed_video_streaming/msg/packet.msg.  Do not edit! */
package ros.pkg.compressed_video_streaming.msg;


import java.nio.ByteBuffer;

public  class packet extends ros.communication.Message
{

  public byte[] blob;
  public int bytes;
  public int b_o_s;
  public int e_o_s;
  public long granulepos;
  public long packetno;

  public packet() {
 super();
  blob = new byte[0];

  }
  public static java.lang.String __s_getDataType() { return "compressed_video_streaming/packet"; }
  public static java.lang.String __s_getMD5Sum() { return "d804434eb295ca184dd6d2e32479185c"; }
  public static java.lang.String __s_getMessageDefinition()
  {
    return 
    "uint8[] blob\n" + 
    "int32 bytes\n" + 
    "int32 b_o_s\n" + 
    "int32 e_o_s\n" + 
    "\n" + 
    "int64 granulepos  \n" + 
    "int64 packetno\n" + 
    "\n" + 
    "";
  }
  public java.lang.String getDataType() { return __s_getDataType(); }
  public java.lang.String getMD5Sum()   { return __s_getMD5Sum(); }
  public java.lang.String getMessageDefinition() { return __s_getMessageDefinition(); }
  public packet clone() {
    packet clone = (packet)super.clone();
      blob =  (byte[])(clone.blob.clone());
    return clone;
  }

  public static java.util.Map<java.lang.String, java.lang.String> fieldTypes() {
         java.util.HashMap<java.lang.String, java.lang.String> m = new java.util.HashMap<java.lang.String, java.lang.String>  ();      m.put("blob", "byte[]");
     m.put("bytes", "int");
     m.put("b_o_s", "int");
     m.put("e_o_s", "int");
     m.put("granulepos", "long");
     m.put("packetno", "long");
     return m;
  }

  public static java.util.Set<java.lang.String> submessageTypes() {
         java.util.HashSet<java.lang.String> s = new java.util.HashSet<java.lang.String>  ();      return s;
  }

  public void setTo(ros.communication.Message __m) {
    if (!(__m instanceof packet)) throw new RuntimeException("Invalid Type");
    packet __m2 = (packet) __m;
    blob = __m2.blob;
    bytes = __m2.bytes;
    b_o_s = __m2.b_o_s;
    e_o_s = __m2.e_o_s;
    granulepos = __m2.granulepos;
    packetno = __m2.packetno;
    }

  public int serializationLength() 
  {
    int __l = 0;
    __l += 4 + (blob.length == 0 ? 0 : blob.length * (1)); // blob
    __l += 4; // bytes
    __l += 4; // b_o_s
    __l += 4; // e_o_s
    __l += 8; // granulepos
    __l += 8; // packetno
    return __l;
  }
  public void serialize(ByteBuffer bb, int seq) {
    bb.putInt(blob.length);
    bb.put(blob);
    bb.putInt(bytes);
    bb.putInt(b_o_s);
    bb.putInt(e_o_s);
    bb.putLong(granulepos);
    bb.putLong(packetno);
  }
  public void deserialize(ByteBuffer bb)  {
     int blob_len = bb.getInt();
    blob = new byte[blob_len];
    bb.get(blob);
    bytes = bb.getInt();
    b_o_s = bb.getInt();
    e_o_s = bb.getInt();
    granulepos = bb.getLong();
    packetno = bb.getLong();
  }
}
