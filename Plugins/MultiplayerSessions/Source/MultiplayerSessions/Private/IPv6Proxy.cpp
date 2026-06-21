#include "IPv6Proxy.h"

#include "Sockets.h"
#include "SocketSubsystem.h"
#include "SocketTypes.h"
#include "HAL/PlatformProcess.h"

FIPv6Proxy::~FIPv6Proxy()
{
	Stop();
}

FString FIPv6Proxy::NormalizeIPv6(const FString& InIPv6)
{
	FString Result = InIPv6.TrimStartAndEnd();

	if (Result.StartsWith(TEXT("[")) && Result.EndsWith(TEXT("]")))
	{
		Result = Result.Mid(1, Result.Len() - 2);
	}

	return Result;
}

bool FIPv6Proxy::CreateAddress(const FString& Ip, int32 Port, TSharedPtr<FInternetAddr>& OutAddr)
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return false;
	}

	bool bIsValid = false;
	TSharedRef<FInternetAddr> Addr = SocketSubsystem->CreateInternetAddr();

	FString CleanIp = Ip.TrimStartAndEnd();
	if (CleanIp.StartsWith(TEXT("[")) && CleanIp.EndsWith(TEXT("]")))
	{
		CleanIp = CleanIp.Mid(1, CleanIp.Len() - 2);
	}

	Addr->SetIp(*CleanIp, bIsValid);
	Addr->SetPort(Port);

	if (!bIsValid)
	{
		return false;
	}

	OutAddr = Addr;
	return true;
}

FSocket* FIPv6Proxy::CreateUdpSocket(
	const FString& Name,
	const FName& Protocol,
	const FString& BindIp,
	int32 BindPort,
	bool bIPv6Only)
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem)
	{
		return nullptr;
	}

	FSocket* Socket = SocketSubsystem->CreateSocket(NAME_DGram, *Name, Protocol);
	if (!Socket)
	{
		return nullptr;
	}

	Socket->SetReuseAddr(true);
	Socket->SetNonBlocking(true);
	Socket->SetRecvErr(false);


	TSharedPtr<FInternetAddr> BindAddr;
	if (!CreateAddress(BindIp, BindPort, BindAddr))
	{
		SocketSubsystem->DestroySocket(Socket);
		return nullptr;
	}

	if (!Socket->Bind(*BindAddr))
	{
		SocketSubsystem->DestroySocket(Socket);
		return nullptr;
	}

	return Socket;
}

bool FIPv6Proxy::StartServer(int32 ListenPort, int32 TargetPort)
{
	Stop();

	if (!CreateAddress(TEXT("127.0.0.1"), TargetPort, TargetIPv4Addr))
	{
		return false;
	}

	// 监听外部 IPv6: [::]:7777
	IPv6Socket = CreateUdpSocket(
		TEXT("IPv6Proxy_Server_IPv6"),
		FNetworkProtocolTypes::IPv6,
		TEXT("::"),
		ListenPort,
		true
	);

	if (!IPv6Socket)
	{
		Stop();
		return false;
	}

	// 给本机 UE 发 IPv4 包，端口用 0 自动分配
	IPv4Socket = CreateUdpSocket(
		TEXT("IPv6Proxy_Server_IPv4"),
		FNetworkProtocolTypes::IPv4,
		TEXT("0.0.0.0"),
		0,
		false
	);

	if (!IPv4Socket)
	{
		Stop();
		return false;
	}

	bRunning = true;

	TaskA = Async(EAsyncExecution::Thread, [this]()
		{
			ServerIPv6ToUE();
		});

	TaskB = Async(EAsyncExecution::Thread, [this]()
		{
			ServerUEToIPv6();
		});

	return true;
}

bool FIPv6Proxy::StartClient(const FString& RemoteIPv6, int32 LocalListenPort, int32 RemotePort)
{
	Stop();

	const FString CleanIPv6 = NormalizeIPv6(RemoteIPv6);

	if (!CreateAddress(CleanIPv6, RemotePort, TargetIPv6Addr))
	{
		return false;
	}

	// UE 客户端连接 127.0.0.1:7777
	IPv4Socket = CreateUdpSocket(
		TEXT("IPv6Proxy_Client_IPv4"),
		FNetworkProtocolTypes::IPv4,
		TEXT("127.0.0.1"),
		LocalListenPort,
		false
	);

	if (!IPv4Socket)
	{
		Stop();
		return false;
	}

	// 用 IPv6 发到主机
	IPv6Socket = CreateUdpSocket(
		TEXT("IPv6Proxy_Client_IPv6"),
		FNetworkProtocolTypes::IPv6,
		TEXT("::"),
		0,
		true
	);

	if (!IPv6Socket)
	{
		Stop();
		return false;
	}

	bRunning = true;

	TaskA = Async(EAsyncExecution::Thread, [this]()
		{
			ClientUEToIPv6();
		});

	TaskB = Async(EAsyncExecution::Thread, [this]()
		{
			ClientIPv6ToUE();
		});

	return true;
}

void FIPv6Proxy::Stop()
{
	bRunning = false;

	if (TaskA.IsValid())
	{
		TaskA.Wait();
	}

	if (TaskB.IsValid())
	{
		TaskB.Wait();
	}

	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	if (SocketSubsystem)
	{
		if (IPv6Socket)
		{
			IPv6Socket->Close();
			SocketSubsystem->DestroySocket(IPv6Socket);
			IPv6Socket = nullptr;
		}

		if (IPv4Socket)
		{
			IPv4Socket->Close();
			SocketSubsystem->DestroySocket(IPv4Socket);
			IPv4Socket = nullptr;
		}
	}

	TargetIPv4Addr.Reset();
	TargetIPv6Addr.Reset();
	LastRemoteIPv6Addr.Reset();
	LastLocalIPv4Addr.Reset();
}

void FIPv6Proxy::ServerIPv6ToUE()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	while (bRunning && IPv6Socket && IPv4Socket)
	{
		uint32 PendingSize = 0;

		if (IPv6Socket->HasPendingData(PendingSize))
		{
			TArray<uint8> Buffer;
			Buffer.SetNumUninitialized(FMath::Min(PendingSize, 65507u));

			int32 BytesRead = 0;
			TSharedRef<FInternetAddr> FromAddr = SocketSubsystem->CreateInternetAddr();

			if (IPv6Socket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *FromAddr))
			{
				Buffer.SetNum(BytesRead);

				{
					FScopeLock Lock(&EndpointMutex);
					LastRemoteIPv6Addr = FromAddr;
				}

				int32 BytesSent = 0;
				IPv4Socket->SendTo(Buffer.GetData(), Buffer.Num(), BytesSent, *TargetIPv4Addr);
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}
}

void FIPv6Proxy::ServerUEToIPv6()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	while (bRunning && IPv6Socket && IPv4Socket)
	{
		uint32 PendingSize = 0;

		if (IPv4Socket->HasPendingData(PendingSize))
		{
			TArray<uint8> Buffer;
			Buffer.SetNumUninitialized(FMath::Min(PendingSize, 65507u));

			int32 BytesRead = 0;
			TSharedRef<FInternetAddr> FromAddr = SocketSubsystem->CreateInternetAddr();

			if (IPv4Socket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *FromAddr))
			{
				Buffer.SetNum(BytesRead);

				TSharedPtr<FInternetAddr> RemoteAddr;
				{
					FScopeLock Lock(&EndpointMutex);
					RemoteAddr = LastRemoteIPv6Addr;
				}

				if (RemoteAddr.IsValid())
				{
					int32 BytesSent = 0;
					IPv6Socket->SendTo(Buffer.GetData(), Buffer.Num(), BytesSent, *RemoteAddr);
				}
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}
}

void FIPv6Proxy::ClientUEToIPv6()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	while (bRunning && IPv6Socket && IPv4Socket)
	{
		uint32 PendingSize = 0;

		if (IPv4Socket->HasPendingData(PendingSize))
		{
			TArray<uint8> Buffer;
			Buffer.SetNumUninitialized(FMath::Min(PendingSize, 65507u));

			int32 BytesRead = 0;
			TSharedRef<FInternetAddr> FromAddr = SocketSubsystem->CreateInternetAddr();

			if (IPv4Socket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *FromAddr))
			{
				Buffer.SetNum(BytesRead);

				{
					FScopeLock Lock(&EndpointMutex);
					LastLocalIPv4Addr = FromAddr;
				}

				int32 BytesSent = 0;
				IPv6Socket->SendTo(Buffer.GetData(), Buffer.Num(), BytesSent, *TargetIPv6Addr);
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}
}

void FIPv6Proxy::ClientIPv6ToUE()
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);

	while (bRunning && IPv6Socket && IPv4Socket)
	{
		uint32 PendingSize = 0;

		if (IPv6Socket->HasPendingData(PendingSize))
		{
			TArray<uint8> Buffer;
			Buffer.SetNumUninitialized(FMath::Min(PendingSize, 65507u));

			int32 BytesRead = 0;
			TSharedRef<FInternetAddr> FromAddr = SocketSubsystem->CreateInternetAddr();

			if (IPv6Socket->RecvFrom(Buffer.GetData(), Buffer.Num(), BytesRead, *FromAddr))
			{
				Buffer.SetNum(BytesRead);

				TSharedPtr<FInternetAddr> LocalAddr;
				{
					FScopeLock Lock(&EndpointMutex);
					LocalAddr = LastLocalIPv4Addr;
				}

				if (LocalAddr.IsValid())
				{
					int32 BytesSent = 0;
					IPv4Socket->SendTo(Buffer.GetData(), Buffer.Num(), BytesSent, *LocalAddr);
				}
			}
		}
		else
		{
			FPlatformProcess::Sleep(0.001f);
		}
	}
}