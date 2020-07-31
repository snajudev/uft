import com.snaju.io.UFTSocket;

class SampleClient
{
	public static void main(String args[])
	{
		if (args.length != 3)
		{
			System.out.println("Invalid args");
			System.out.println("java SampleClient host /path/to/local/source /path/on/remote/destination");

			return;
		}

		System.load(System.getProperty("user.dir") + "/UFT/libJUFT.so");

		UFTSocket socket = new UFTSocket();

		socket.setTimeout(10 * 1000);

		//               127 0  0  1
		//                7F 00 00 01
		if (socket.connect(0x7F000001, 9000))
		{
			if (socket.setBlocking(true))
			{
				System.out.println(String.format("Sending %s to %s", args[1], args[2]));

				if (socket.sendFile(args[1], args[2]) <= 0)
				{

					System.out.println(String.format("Error sending %s to %s", args[1], args[2]));
				}
			}

			socket.close();
		}
	}
}
