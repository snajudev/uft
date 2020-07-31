import com.snaju.io.UFTSocket;

class SampleServer
{
	public static void main(String args[])
	{
		System.load(System.getProperty("user.dir") + "/UFT/libJUFT.so");

		UFTSocket listener = new UFTSocket();

		//                127 0  0  1
		//                 7F 00 00 01
		if (listener.listen(0x7F000001, 9000))
		{
			if (listener.setBlocking(true))
			{
				UFTSocket client;

				while ((client = listener.accept()) != null)
				{
					client.setTimeout(10 * 1000);

					for (int i = 0; i < args.length; i += 2)
					{
						long fileSize;

						if ((fileSize = client.sendFile(args[i], args[i + 1])) <= 0)
						{
							System.out.println(String.format("Error sending %s to %s", args[i], args[i + 1]));

							client.close();

							break;
						}

						System.out.println(String.format("Sent %s to %s", args[i], args[i + 1]));
					}

					while (client.isConnected())
					{
						long fileSize;
						StringBuffer recvFilePath = new StringBuffer();

						while ((fileSize = client.receiveFile(recvFilePath)) > 0)
						{

							System.out.println(String.format("Received %s (%d bytes)", recvFilePath, fileSize));
						}

						if (fileSize == 0)
						{

							System.out.println("Connection closed");
						}
						else if (fileSize == -2)
						{

							System.out.println("Internal API Error");
						}
					}

					client.close();

					System.out.println("Client disconnected");
				}
			}
			
			listener.close();
		}
	}
}
