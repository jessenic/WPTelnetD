#pragma once

#include "pch.h"

using namespace Platform;

namespace CSDDiagRT
{
	//namespace CoreAPI 
	//{		
		public ref class ProcessInfo sealed{
			private:
				PROCESS_INFORMATION _info;
			internal:
				ProcessInfo(PROCESS_INFORMATION pInfo) ;
		};

		/*public ref class CETWLogging sealed{
		private:
			PROCESS_INFORMATION _info;
		public:
			CETWLogging();
		};*/

		public ref class CCSDDiagRT sealed
		{
			public:
				static int CreateProcess(String ^pCmdLine);
				static int GetLastError();
				static String^ CCSDDiagRT::GetCommandLine();

				static bool TelnetProcessConnection(int pSocket, String^ pWelcomeInfo);
				static int TelnetListenForOneConnection(int pPort, int* pSocket, int *pWsaError);
				static int TelnetConnectTo(String^ pIpAddress, int pPort,int *pSocket, int *pWsaError);
				static bool TelnetInitNetworking(int *pWsaError);
				static bool TelnetShutDownNetworking();
		};

		
	//};
};