import java.io.*;
import java.net.*;

class Cant_Construct_From_Bytes extends Exception {
    public Cant_Construct_From_Bytes(String msg){
        super(msg);
    }
}


/**
 * A message sent to me to request my current timestamp.
 */
class Request_Time {
    public static final byte LENGTH = 16;
    public static final byte MSG_ID = 1;
    /** The time at which this message was sent. */
    public long cam_host_usec;

    /**
     * Construct a Request_Time object from a byte stream, most likely sent
     * as a UDP message.
     */
    public Request_Time(int in_length, byte [] in)
                                            throws Cant_Construct_From_Bytes {
        if (in_length != LENGTH ||
            in[0] != MSG_ID) {
            throw new Cant_Construct_From_Bytes("Request_Time: length= " + in_length + " msg_id= " + in[0]);
        }
        cam_host_usec = ((0xff & (long)in[15]) << 56) |
                        ((0xff & (long)in[14]) << 48) |
                        ((0xff & (long)in[13]) << 40) |
                        ((0xff & (long)in[12]) << 32) |
                        ((0xff & (long)in[11]) << 24) |
                        ((0xff & (long)in[10]) << 16) |
                        ((0xff & (long)in[9]) << 8) |
                        ((0xff & (long)in[8]));
    }

    /**
     * Convert this record to a byte stream.
     */
    public byte [] to_bytes() {
        byte [] out = new byte[LENGTH];
        out[0] = MSG_ID;
        out[1] = 0;
        out[2] = 0;
        out[3] = 0;
        out[4] = 0;
        out[5] = 0;
        out[6] = 0;
        out[7] = 0;
        out[8] = (byte)cam_host_usec;
        out[9] = (byte)(cam_host_usec >> 8);
        out[10] = (byte)(cam_host_usec >> 16);
        out[11] = (byte)(cam_host_usec >> 24);
        out[12] = (byte)(cam_host_usec >> 32);
        out[13] = (byte)(cam_host_usec >> 40);
        out[14] = (byte)(cam_host_usec >> 48);
        out[15] = (byte)(cam_host_usec >> 56);
        return out;
    }
}


/**
 * A message I send in response to Request_Time.  Echoes my current time back to
 * the requestor.
 */
class Echo_Time {
    public static final byte LENGTH = 24;
    public static final byte MSG_ID = 2;
    /** Cam_host_usec field from the original Request_Time message. */
    public long cam_host_usec;
    /** My current time. */
    public long client_msec;

    Echo_Time(long c, long s) {
        this.cam_host_usec = c;
        this.client_msec = s;
    }

    /**
     * Convert this record to a byte stream.
     */
    public byte [] to_bytes() {
        byte [] out = new byte[LENGTH];
        out[0] = MSG_ID;
        out[1] = 0;
        out[2] = 0;
        out[3] = 0;
        out[4] = 0;
        out[5] = 0;
        out[6] = 0;
        out[7] = 0;

        out[8] = (byte)cam_host_usec;
        out[9] = (byte)(cam_host_usec >> 8);
        out[10] = (byte)(cam_host_usec >> 16);
        out[11] = (byte)(cam_host_usec >> 24);
        out[12] = (byte)(cam_host_usec >> 32);
        out[13] = (byte)(cam_host_usec >> 40);
        out[14] = (byte)(cam_host_usec >> 48);
        out[15] = (byte)(cam_host_usec >> 56);

        out[16] = (byte)client_msec;
        out[17] = (byte)(client_msec >> 8);
        out[18] = (byte)(client_msec >> 16);
        out[19] = (byte)(client_msec >> 24);
        out[20] = (byte)(client_msec >> 32);
        out[21] = (byte)(client_msec >> 40);
        out[22] = (byte)(client_msec >> 48);
        out[23] = (byte)(client_msec >> 56);
        return out;
    }
}

/**
 * Describes a single color blob.
 */
class Blob_Stats {
    public short min_x;
    public short max_x;
    public short min_y;
    public short max_y;
    public int sum_x;
    public int sum_y;
    public int count;
}


/**
 * Contains all the detected bounding boxes.
 */
class Udp_Blob_List {
    public static final byte MIN_LENGTH = 16;
    public static final byte MSG_ID = 2;
    public int blob_count;
    public long client_msec;
    public Blob_Stats[] blob;

    /**
     * Construct a Udp_Blob_List object from a byte stream, most likely sent
     * as a UDP message.
     */
    public Udp_Blob_List(int in_length, byte [] in)
                                            throws Cant_Construct_From_Bytes {
        final int OFFSET_OF_BLOB_STATS = 16;
        final int SIZE_OF_BLOB_STATS = 20;
        int ii;
        if (in_length < MIN_LENGTH ||
            in[0] != MSG_ID) {
            throw new Cant_Construct_From_Bytes("Udp_Blob: length= " + in_length + " msg_id= " + in[0]);
        }
        blob_count = (in_length - OFFSET_OF_BLOB_STATS) / SIZE_OF_BLOB_STATS;
        /*
        blob_count = ((0xff & (int)in[7]) << 24) |
                     ((0xff & (int)in[6]) << 16) |
                     ((0xff & (int)in[5]) << 8) |
                     ((0xff & (int)in[4]));
        if (in_length < OFFSET_OF_BLOB_STATS + blob_count * SIZE_OF_BLOB_STATS) {
            throw new Cant_Construct_From_Bytes("Udp_Blob: length= " + in_length + " blob_count= " + blob_count);
        }
        */

        client_msec = ((0xff & (long)in[15]) << 56) |
                      ((0xff & (long)in[14]) << 48) |
                      ((0xff & (long)in[13]) << 40) |
                      ((0xff & (long)in[12]) << 32) |
                      ((0xff & (long)in[11]) << 24) |
                      ((0xff & (long)in[10]) << 16) |
                      ((0xff & (long)in[9]) << 8) |
                      ((0xff & (long)in[8]));
        blob = new Blob_Stats[blob_count];
        for (ii = 0; ii < blob_count; ++ii) {
            final int ind = ii * SIZE_OF_BLOB_STATS + OFFSET_OF_BLOB_STATS;
            blob[ii] = new Blob_Stats();
            blob[ii].min_x = (short)(((0xff & (int)in[ind + 1]) << 8) |
                             ((0xff & (int)in[ind + 0])));
            blob[ii].max_x = (short)(((0xff & (int)in[ind + 3]) << 8) |
                             ((0xff & (int)in[ind + 2])));
            blob[ii].min_y = (short)(((0xff & (int)in[ind + 5]) << 8) |
                             ((0xff & (int)in[ind + 4])));
            blob[ii].max_y = (short)(((0xff & (int)in[ind + 7]) << 8) |
                             ((0xff & (int)in[ind + 6])));
            blob[ii].sum_x = ((0xff & (int)in[ind + 11]) << 24) |
                             ((0xff & (int)in[ind + 10]) << 16) |
                             ((0xff & (int)in[ind + 9]) << 8) |
                             ((0xff & (int)in[ind + 8]));
            blob[ii].sum_y = ((0xff & (int)in[ind + 15]) << 24) |
                             ((0xff & (int)in[ind + 14]) << 16) |
                             ((0xff & (int)in[ind + 13]) << 8) |
                             ((0xff & (int)in[ind + 12]));
            blob[ii].count = ((0xff & (int)in[ind + 19]) << 24) |
                             ((0xff & (int)in[ind + 18]) << 16) |
                             ((0xff & (int)in[ind + 17]) << 8) |
                             ((0xff & (int)in[ind + 16]));
        }
    }
}

class TestUDPClient
{
    public static void attempt_connection(DatagramSocket socket,
                                          String picam_hostname,
                                          int picam_port) throws Exception
    {
        // Send Echo_Time (with cam_host_usec == 0) to establish connection

        long now = System.currentTimeMillis();
        Echo_Time msg_out = new Echo_Time(0, now);
        InetAddress ip_addr = InetAddress.getByName(picam_hostname);
        byte[] out_bytes = msg_out.to_bytes();
        DatagramPacket out_packet =
                       new DatagramPacket(out_bytes, out_bytes.length,
                                          ip_addr, picam_port);
        socket.send(out_packet);
    }

    public static void main(String args[]) throws Exception
    {
        final int MY_PORT = 10696;
        final int PICAM_PORT = 9696;
        final String PICAM_HOSTNAME = "raspberrypi.local";
        boolean connected = false;
        /* MulticastSocket is the preferred way to set SO_REUSEPORT on a
           DatagramSocket. */
        MulticastSocket socket = new MulticastSocket(MY_PORT);
        byte[] in_bytes = new byte[256];
        attempt_connection(socket, PICAM_HOSTNAME, PICAM_PORT);

        socket.setSoTimeout(1000);

        while(true)
        {
            DatagramPacket in_packet = new DatagramPacket(in_bytes,
                                                          in_bytes.length);
            try {
                socket.receive(in_packet);

                byte msg_id = in_packet.getData()[0];
                if (msg_id == Request_Time.MSG_ID) {
                    Request_Time msg_in = new Request_Time(
                                                    in_packet.getLength(),
                                                    in_packet.getData());
                    InetAddress ip_addr = in_packet.getAddress();
                    int port = in_packet.getPort();
                    long now = System.currentTimeMillis();
                    Echo_Time msg_out = new Echo_Time(msg_in.cam_host_usec, now);
                    byte[] out_bytes = msg_out.to_bytes();
                    DatagramPacket out_packet =
                               new DatagramPacket(out_bytes, out_bytes.length,
                                                  ip_addr, port);
                    socket.send(out_packet);
                    if (!connected) {
                        System.out.println("synching clocks with " +
                                           ip_addr.getHostAddress() +
                                           " port " + port);
                        connected = true;
                    }
                    System.out.println("saw " + msg_in.cam_host_usec +
                                       " sent " + now);
                } else if (msg_id == Udp_Blob_List.MSG_ID) {
                    int ii;
                    Udp_Blob_List msg_in = new Udp_Blob_List(
                                                    in_packet.getLength(),
                                                    in_packet.getData());
                    System.out.println("===============");
                    System.out.println("blob_count:  " + msg_in.blob_count);
                    System.out.println("client_msec: " + msg_in.client_msec);
                    for (ii = 0; ii < msg_in.blob_count; ++ii) {
                        System.out.println("  bbox:  (" +
                                           msg_in.blob[ii].min_x + " " +
                                           msg_in.blob[ii].min_y + " " +
                                           msg_in.blob[ii].max_x + " " +
                                           msg_in.blob[ii].max_y + ")");
                        System.out.println("  sum:   (" +
                                           msg_in.blob[ii].sum_x + " " +
                                           msg_in.blob[ii].sum_y + ")");
                        System.out.println("  count: " +
                                           msg_in.blob[ii].count);
                    }
                } else {
                    System.out.println("Bad packet msg_id " + msg_id);
                }
            } catch (SocketTimeoutException e) {
                if (connected) {
                    System.out.println("timeout; still waiting");
                } else {
                    System.out.println("timeout; retrying");
                    attempt_connection(socket, PICAM_HOSTNAME, PICAM_PORT);
                }
            }
        }
    }
}
