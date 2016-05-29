using System;
using System.Net.Sockets;
using System.Net;
using System.Collections.Generic;
using System.Collections;
using System.Threading;
using System.Text;
using System.Linq;

namespace ChatClient
{
	
		
	class Client{

		private string _ip;
		private int _port;
		private bool _isConnected = false;

		private TcpClient _cli;

		public Client(string ip, int port){
			this._ip = ip;
			this._port = port;

		}

		public bool isConnected(){
			return this._isConnected;
		}

		public void Connect(){
			
			try{
				Console.WriteLine(IPAddress.Parse(this._ip));

				_cli = new TcpClient (new IPEndPoint (IPAddress.Parse (this._ip), this._port));
				_cli.Connect("localhost", this._port);

				Console.WriteLine("Connected");
				_isConnected = true;
			}
			catch(Exception ex){
				Console.WriteLine (ex.Message);
			}
		}
			
		public NetworkStream getStream(){
			return _cli.GetStream ();
		}

		public void Disconnect(){
			if (_cli.Connected) {
				_cli.Close ();
			}

			_isConnected = false;
		}

	}

	static public class ThreadTasks{

		static private Mutex mut;
		static private NetworkStream sharedStream;
		static private bool stopAllThreads = false;
		static private byte[] incomingData = new byte[1014];
		static private ManualResetEvent recDone = new ManualResetEvent (false);

		static public void InitialiseMutex(string name){
			if (mut == null) {
				mut = new Mutex (false, name);
			}
		}

		static public void StopAllThreads(){
			mut.WaitOne();
			stopAllThreads = true;
			mut.ReleaseMutex();
		}

		static public bool ThreadsRunning(){
			bool rc = false;
			mut.WaitOne (); //wait till mutex is free then lock it
			rc = !stopAllThreads;
			mut.ReleaseMutex ();
			return rc;
		}

		static public void CreatedSharedNetworkStream(NetworkStream stream){

			if (mut == null) {
				sharedStream = stream;
			} else {
				mut.WaitOne ();
				sharedStream = stream;
				mut.ReleaseMutex ();
			}
		}

		static public void DestroyMutex(){
			StopAllThreads ();
			mut.Dispose ();
		}

		static private void ReadComplete(IAsyncResult ar){
			mut.WaitOne ();
			string reply;
			int incomingDataLength;

			try{
			  incomingDataLength = sharedStream.EndRead (ar);
			}
			catch(Exception e){
				Console.WriteLine (e.Message);
				return;
			}

			mut.ReleaseMutex ();

			if (incomingDataLength > 0) {
				reply = Encoding.ASCII.GetString (incomingData);

				int i = string.Compare (reply, "Beat");
				//Console.Write (String.Format ("{0} - {1}", i, reply));

				if (i == 0) {
					//Console.WriteLine ("beat recd");
					SendUsingSharedStream ("BeatReply");
				} else {
					Console.WriteLine (String.Format ("\nRECD: {0}", reply));
					Console.Write ("SEND > ");
				}

			}
			recDone.Set ();

			Array.Clear (incomingData, 0, incomingData.Length);
		}

		static public void ListenThread(){
			Console.WriteLine ("ListenThread starting");

			AsyncCallback handle = new AsyncCallback (ReadComplete);

			while (ThreadsRunning ()) {

				mut.WaitOne ();

				try{
					sharedStream.BeginRead (incomingData, 0, incomingData.Length, handle, sharedStream);
				}
				catch(Exception e){
					Console.WriteLine (e.Message);
				}

				mut.ReleaseMutex ();
				recDone.WaitOne ();

			}
			Console.WriteLine ("ListenThread ending");
		}

		static private void SendUsingSharedStream(string command){
			mut.WaitOne ();
			sharedStream.Write (Encoding.ASCII.GetBytes (command), 0, command.Length);
			mut.ReleaseMutex ();
		}

		static public void SendThread(){
			Console.WriteLine ("SendThread starting");
			while (ThreadsRunning()) {

				Console.Write("SEND > ");
				string command = Console.ReadLine ();

				string[] elements = command.Split (' ');

				switch (elements[0]) {

				case "CLOSE":
					StopAllThreads ();
					mut.WaitOne ();
					sharedStream.Close ();
					mut.ReleaseMutex ();
					break;

				case "NEWUSER":
				case "LOGIN":
				case "PING":
				case "SEND":
				case "LIST":
					SendUsingSharedStream (command);
					break;

				default:
					Console.WriteLine ("Invalid Command Typed");
					break;

				}
					
			}
			Console.WriteLine ("SendThread ending");
		}
	}
		

	class MainClass
	{
		public static void Main (string[] args)
		{
			Console.WriteLine ("Hello World!");
			Client cli = new Client ("127.0.0.1", 51718);

			cli.Connect ();
			if (!cli.isConnected ()) {
				return;
			}

			ThreadTasks.CreatedSharedNetworkStream (cli.getStream ());
			ThreadTasks.InitialiseMutex ("stream");

			Thread listenThread = new Thread(new ThreadStart(ThreadTasks.ListenThread));
			Thread sendThread = new Thread (new ThreadStart (ThreadTasks.SendThread));

			listenThread.Start ();
			sendThread.Start ();

			while (ThreadTasks.ThreadsRunning ()) {

			}

			listenThread.Abort ();
			sendThread.Abort ();

			listenThread.Join ();
			sendThread.Join ();

			ThreadTasks.DestroyMutex ();

			cli.Disconnect ();
		}
	}
}
