// <copyright file="ctrlc.cs" company="SpongySoft">
//     Copyright (c) SpongySoft. All rights reserved.
// </copyright>

namespace SpongySoft
{
	using System;
	using System.Threading;
	using System.Runtime.InteropServices;

	public class ControlCSuppressor : IDisposable
	{
        /// <summary>
        /// An object used to guarantee thread synchronization.
        /// </summary>
		private object sync = new object();

		/// <summary>
		/// A value indicating whether Ctrl-C was hit suring the time that it was disabled.
		/// </summary>
		private bool ctrlCWasPressed = false;

		/// <summary>
		/// A value indicating whether we are currently capturing/processing Ctrl-C events.
		/// </summary>
		private bool capturing = false;

		/// <summary>
		/// A delegate that represents the Ctrl-C callback routine.
		/// </summary>
        private HandlerRoutine handlerCallback;

		/// <summary>
		/// Constructor.
		/// </summary>
		public ControlCSuppressor()
		{
			// create the delegate
			this.handlerCallback = new HandlerRoutine(this.CtrlCHandlerRoutine);
		}

		/// <summary>
		/// Start processing Ctrl-C keystroke events and log them, but don't pass them on to the system.
		/// </summary>
		public void PauseControlCProcessing()
		{
			if (!this.capturing)
			{
				lock (this.sync)
				{
					this.ctrlCWasPressed = false;
					this.capturing = true;
					NativeMethods.SetConsoleCtrlHandler(this.handlerCallback, true);
				}
			}
		}

		/// <summary>
		/// Resumes the normal state of Control-C handling.
		/// </summary>
		/// <returns>A value indicating whether Ctrl-C was hit suring the time that it was disabled.</returns>
		public bool ResumeControlCProcessing()
		{
			if (this.capturing)
			{
				lock (this.sync)
				{
					this.capturing = false;
					NativeMethods.SetConsoleCtrlHandler(this.handlerCallback, false);
				}
			}

			return this.ctrlCWasPressed;
		}

		/// <summary>
		/// Internal callback routine to handle control keystroke events.
		/// </summary>
		/// <param name="ctrlType">The control key hit.</param>
		/// <returns>A value indicating whether the keystroke was handled here. If not, the system should handle it.</returns>
		private bool CtrlCHandlerRoutine( ConsoleSpecialKey ctrlType )
		{
			lock (this.sync)
			{
				if (ctrlType == ConsoleSpecialKey.ControlC)
				{
					this.ctrlCWasPressed = true;
					return true;
				}
			}

			return false;
		}

		/// <summary>
		/// Dispose of resources.
		/// </summary>
		public void Dispose()
		{
			Dispose( true );
			GC.SuppressFinalize( this );
		}

		/// <summary>
		/// Dispose of resources.
		/// </summary>
		/// <param name="disposing">A value indicating whether </param>
		protected virtual void Dispose( bool disposing )
		{
			this.ResumeControlCProcessing();
		}

		/// <summary>
		/// Define a delegate for the callback.
		/// </summary>
		[return: MarshalAs( UnmanagedType.Bool )]
		internal delegate bool HandlerRoutine( ConsoleSpecialKey ctrlType );

		/// <summary>
		/// Provide a class for unmanaged access.
		/// </summary>
		internal static partial class NativeMethods
		{
			[return: MarshalAs( UnmanagedType.Bool )]
			[DllImport( "kernel32.dll", SetLastError = true )]
			internal static extern bool SetConsoleCtrlHandler( HandlerRoutine handler, [MarshalAs( UnmanagedType.Bool )] bool add );
		}
	}
}

