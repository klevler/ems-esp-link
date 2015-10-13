package strolch.interfaces.esplink;

/*
 **	javac -cp syslog4j-0.9.46-bin.jar strolch/interfaces/esplink/EMSSyslog.java
 **	jar cvfe EMSSyslog.jar strolch/interfaces/esplink/EMSSyslog  strolch/interfaces/esplink/EMSSyslog.class syslog4j-0.9.46-bin.jar
 **	java -jar EMSSyslog.jar <ems-link> <syslog-server>
 */

//import java.lang.*;
import java.io.*;
import java.net.*;
import java.text.SimpleDateFormat;
import java.util.Date;
import org.productivity.java.syslog4j.*; // http://www.syslog4j.org/

class EMSPktHeader {
	public long sntpTimestamp= 0;	// SNTP timestamp from EMSLink
	public long espTickCount = 0;	// ESP8266 system ticker (1ms)
	public int emsPkgLength = 0;
	
	// validate EMSLink package header
	public boolean validate() {
		if (this.emsPkgLength > 128 || this.emsPkgLength < 2)
			return false;
		return true;
	}
	
	// convert package header to String
	public String toString() {
		SimpleDateFormat sdf = new SimpleDateFormat("yy-MM-dd H:mm:ss"); 
		return new String(String.format("%s %d.%03d [%d] ",
				  			sdf.format(new Date(this.sntpTimestamp * 1000)),	// to millis...
				  			this.espTickCount / 1000, 
				  			this.espTickCount % 1000,
				  			this.emsPkgLength));
	}
}

public class EMSSyslog {

	private String emsServer;
	private int emsPort;

	private static String syslogServer = "192.168.254.20";
	private static int syslogPort = 514;
	private static int useSyslog = 1; // use syslog instead of stdout
	static SyslogIF syslog;
	
	private Socket skt;
	private BufferedInputStream in;

	private static int socketConnectTimeout = 5000;
	private static int socketDataTimeout = 30000;

	private int debugLevel = 1;

	private static int byteCount = 0;
	private int[] data = new int[512];

	// simple bytes to string converter
	final protected static char[] HEXARRAY = "0123456789ABCDEF".toCharArray();
	
	// convenience...
	private static final int EMSPKG_HEADER_SIZE = 10;	// Timestamp, ESP Ticker, telegram size
	private static final int EMSPKG_EOD_SIZE= 4;		// crc, brk, 0xe51a
	private static final int EMSPKG_MIN_SIZE= 3;		// minimum size we'll care about 
	

	public static String bytesToHex(int[] data, int offset, int count, boolean separator) {
		
		char[] hexChars = new char[count * 3];
		for (int j = 0; j < count; j++) {
			int v = data[j + offset] & 0xFF;
			hexChars[j * 3] 	= HEXARRAY[v >>> 4];
			hexChars[j * 3 + 1] = HEXARRAY[v & 0x0F];
			hexChars[j * 3 + 2] = ' ';
		}
		return new String(hexChars);
	}

	public static String bytesToHex(int[] bytes, boolean separator) {
		return bytesToHex(bytes, 0, bytes.length, true);
	}
	
	public static String bytesToHex(int[] bytes, int offset, boolean separator) {
		return bytesToHex(bytes, offset, bytes.length - offset, true);
	}
	
	public int getDebugLevel() {
		return debugLevel;
	}

	public void setDebugLevel(int debug) {
		this.debugLevel = debug;
	}

	public EMSSyslog(String emsServer, int emsPort) {
		this.emsServer = emsServer;
		this.emsPort = emsPort;
		this.skt = null;
	}

	public void open() throws IOException {
		skt = new Socket();
		skt.connect(new InetSocketAddress(emsServer, emsPort), socketConnectTimeout);
		skt.setSoTimeout(socketDataTimeout);
		in = new BufferedInputStream(skt.getInputStream());
	}

	public void close() throws IOException {
		if (in != null) {
			in.close();
			in = null;
		}
		if (skt != null) {
			skt.close();
			skt = null;
		}
	}

	public boolean isConnected() {
		return skt != null;
	}

	private void waitForEndOfFrame() throws IOException {
		int lastC = 0, c = 0;
		do {
			lastC = c;
			c = in.read();
			byteCount++;
			if (c < 0)
				throw new IOException("End of input stream");
		} while ((lastC != 0xe5) && (c != 0x1a));
	}

	public int fetchByte() throws IOException {
		int _data = in.read();
		if (_data < 0)
			throw new IOException("End of input stream");
		byteCount++;
		return _data & 0xff;
	}
	
	public int[] getTelegramm() throws IOException {
		
		try {
			if (!isConnected()) {
				syslog.notice("connecting to " + emsServer + ":" + emsPort);
				open();
			}

			EMSPktHeader eph = new EMSPktHeader();
			
			try {
				int rawPkgLength;
				int payloadLength;
				
				do {
					rawPkgLength = 0;

					// *** read protocol header
					// int res = in.read(byte[] ba, 0, EMSPKG_HEADER_SIZE);
					// if (res < 0)
					//   throw new IOException("End of input stream");
					// byteCount += EMSPKG_HEADER_SIZE;

					for (; rawPkgLength < EMSPKG_HEADER_SIZE; rawPkgLength++)
						data[rawPkgLength] = fetchByte();
					
					// pick package length from pkgHeader
					eph.emsPkgLength = ((data[9] & 0xFF) << 8  | (data[8] & 0xFF) << 0);
					eph.sntpTimestamp= ((data[3] & 0xFF) << 24 | (data[2] & 0xFF) << 16 | (data[1] & 0xFF) << 8  | (data[0] & 0xFF) << 0);
					eph.espTickCount = ((data[7] & 0xFF) << 24 | (data[6] & 0xFF) << 16 | (data[5] & 0xFF) << 8  | (data[4] & 0xFF) << 0);
					
					// validate package header
					if (! eph.validate()) {
						syslog.error("invalid EMS package header");
						syslog.error(eph.toString() + bytesToHex(data, 0, rawPkgLength, true));
						waitForEndOfFrame();
						return null;
					}
					
					// fetch payload data
					for (; rawPkgLength < EMSPKG_HEADER_SIZE + eph.emsPkgLength; rawPkgLength++)
						data[rawPkgLength] = fetchByte();

					// validate incoming data
					if ((data[rawPkgLength - 2] != 0xe5) && (data[rawPkgLength - 1] != 0x1a)) {
						if (debugLevel >= 1) {
							syslog.error("missing end of frame signature");
							syslog.error(eph.toString() + bytesToHex(data, 0, rawPkgLength, true));
						}
						waitForEndOfFrame();
						return null;
					}

					if (eph.emsPkgLength != rawPkgLength - EMSPKG_HEADER_SIZE) {
						if (debugLevel >= 1) {
							syslog.error(String.format("length mismatch: %d vs %d", eph.emsPkgLength, rawPkgLength));
							syslog.error(eph.toString() + bytesToHex(data, 0, rawPkgLength, true));
						}
						waitForEndOfFrame();
						return null;
					}
					payloadLength = rawPkgLength - (EMSPKG_HEADER_SIZE + EMSPKG_EOD_SIZE);
				} while (payloadLength < EMSPKG_MIN_SIZE);

				int crc = buderusEmsCrc(data, EMSPKG_HEADER_SIZE, EMSPKG_HEADER_SIZE + payloadLength);

				// show incoming frame on crc mismatch
				if (debugLevel >= 2 || (crc != data[EMSPKG_HEADER_SIZE + payloadLength])) {
					if (crc != data[EMSPKG_HEADER_SIZE + payloadLength]) {
						syslog.error(String.format("CRC mismatch: %02x vs %02x", crc, data[EMSPKG_HEADER_SIZE + payloadLength]));
						syslog.error(eph.toString() + bytesToHex(data, 0, rawPkgLength, true));
					} else {
						syslog.debug(String.format("computed CRC: %02x", crc));
						syslog.debug(eph.toString() + bytesToHex(data, 0, rawPkgLength, true));
					}
				}

				if (crc != data[EMSPKG_HEADER_SIZE + payloadLength]) {
					waitForEndOfFrame();
					return null;
				}

				int telegram[] = new int[payloadLength];

				for (int j = 0; j < payloadLength; j++) {
					telegram[j] = (byte) data[j + EMSPKG_HEADER_SIZE];
				}

				return telegram;
				
			} catch (SocketTimeoutException e) {
				throw new IOException("timeout after " + byteCount + " bytes");
			}

		} catch (IOException e) {
			close();
			syslog.emergency(String.format("connection closed: %s", e.toString()));
			byteCount = 0;
			throw e;
		}
	}

	private final static boolean buderusEmsCrcTable = false;
	private final static int buderusEmsPoly = 12;

	private final static int buderusCrcTable[] = { 0x00, 0x02, 0x04, 0x06,
		0x08, 0x0A, 0x0C, 0x0E, 0x10, 0x12, 0x14, 0x16, 0x18, 0x1A, 0x1C,
		0x1E, 0x20, 0x22, 0x24, 0x26, 0x28, 0x2A, 0x2C, 0x2E, 0x30, 0x32,
		0x34, 0x36, 0x38, 0x3A, 0x3C, 0x3E, 0x40, 0x42, 0x44, 0x46, 0x48,
		0x4A, 0x4C, 0x4E, 0x50, 0x52, 0x54, 0x56, 0x58, 0x5A, 0x5C, 0x5E,
		0x60, 0x62, 0x64, 0x66, 0x68, 0x6A, 0x6C, 0x6E, 0x70, 0x72, 0x74,
		0x76, 0x78, 0x7A, 0x7C, 0x7E, 0x80, 0x82, 0x84, 0x86, 0x88, 0x8A,
		0x8C, 0x8E, 0x90, 0x92, 0x94, 0x96, 0x98, 0x9A, 0x9C, 0x9E, 0xA0,
		0xA2, 0xA4, 0xA6, 0xA8, 0xAA, 0xAC, 0xAE, 0xB0, 0xB2, 0xB4, 0xB6,
		0xB8, 0xBA, 0xBC, 0xBE, 0xC0, 0xC2, 0xC4, 0xC6, 0xC8, 0xCA, 0xCC,
		0xCE, 0xD0, 0xD2, 0xD4, 0xD6, 0xD8, 0xDA, 0xDC, 0xDE, 0xE0, 0xE2,
		0xE4, 0xE6, 0xE8, 0xEA, 0xEC, 0xEE, 0xF0, 0xF2, 0xF4, 0xF6, 0xF8,
		0xFA, 0xFC, 0xFE, 0x19, 0x1B, 0x1D, 0x1F, 0x11, 0x13, 0x15, 0x17,
		0x09, 0x0B, 0x0D, 0x0F, 0x01, 0x03, 0x05, 0x07, 0x39, 0x3B, 0x3D,
		0x3F, 0x31, 0x33, 0x35, 0x37, 0x29, 0x2B, 0x2D, 0x2F, 0x21, 0x23,
		0x25, 0x27, 0x59, 0x5B, 0x5D, 0x5F, 0x51, 0x53, 0x55, 0x57, 0x49,
		0x4B, 0x4D, 0x4F, 0x41, 0x43, 0x45, 0x47, 0x79, 0x7B, 0x7D, 0x7F,
		0x71, 0x73, 0x75, 0x77, 0x69, 0x6B, 0x6D, 0x6F, 0x61, 0x63, 0x65,
		0x67, 0x99, 0x9B, 0x9D, 0x9F, 0x91, 0x93, 0x95, 0x97, 0x89, 0x8B,
		0x8D, 0x8F, 0x81, 0x83, 0x85, 0x87, 0xB9, 0xBB, 0xBD, 0xBF, 0xB1,
		0xB3, 0xB5, 0xB7, 0xA9, 0xAB, 0xAD, 0xAF, 0xA1, 0xA3, 0xA5, 0xA7,
		0xD9, 0xDB, 0xDD, 0xDF, 0xD1, 0xD3, 0xD5, 0xD7, 0xC9, 0xCB, 0xCD,
		0xCF, 0xC1, 0xC3, 0xC5, 0xC7, 0xF9, 0xFB, 0xFD, 0xFF, 0xF1, 0xF3,
		0xF5, 0xF7, 0xE9, 0xEB, 0xED, 0xEF, 0xE1, 0xE3, 0xE5, 0xE7 };

	private static int buderusEmsCrc(int[] data2, int from, int to) {
		int crc = 0;
		int d;

		for (int i = from; i < to; i++) {
			if (buderusEmsCrcTable) {
				crc = buderusCrcTable[crc];
			} else {
				d = 0;
				if ((crc & 0x80) != 0) {
					crc ^= buderusEmsPoly;
					d = 1;
				}
				crc = (crc << 1) & 0xfe;
				crc |= d;
			}
			// crc ^= c[i];
			crc = (crc ^ data2[i]) & 0xff;
		}
		return crc;
	}

	/*
	 * args[1]: EMS-ESP-Link Gateway 
	 * args[2]: Syslog-Server
	 */

	public static void main(String args[]) {
		int[] b;

		EMSSyslog emsBus = new EMSSyslog(args.length > 0 ? args[0] : "192.168.254.115", 23);
		SyslogConfigIF config = Syslog.getInstance("udp").getConfig(); // create the Syslog config

		config.setHost(args.length > 1 ? args[1] : syslogServer); 	// Diskstation is running the syslog daemon
		config.setPort(syslogPort); 								// default syslog port
		config.setIdent("EMSLink");
		config.setLocalName("ems-link");
		config.setSendLocalName(true);

		Syslog.createInstance("EMSBus", config);
		syslog = Syslog.getInstance("EMSBus"); 	// create a syslog instance

		syslog.notice("starting EMSLink monitor");

		while (true) {
			try {
				b = emsBus.getTelegramm();

				if (b == null) {
					if (useSyslog == 1) {
					} else {
						System.out.print((new Date()) + "    ");
						System.out.print("Receive Error");
						System.out.print('\n');
						System.out.flush();
					}
				} else {
					if (b.length > 3) {
						if (useSyslog == 1) {
							syslog.info(bytesToHex(b, true));
						} else {
							System.out.print((new Date()) + "    " 
												+ bytesToHex(b, true));
							System.out.print('\n');
							System.out.flush();
						}
					}
				}
			} catch (IOException e) {
				System.out.println(e.toString());
				syslog.warn(e.toString());
			}
		}
	}
}