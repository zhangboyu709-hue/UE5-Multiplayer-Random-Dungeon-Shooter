#pragma once

#include "CoreMinimal.h"
#include "Async/Async.h"

class FSocket;
class FInternetAddr;

class FIPv6Proxy
{
public:
	FIPv6Proxy() = default;
	~FIPv6Proxy();

	bool StartServer(int32 ListenPort = 7777, int32 TargetPort = 7777);
	bool StartClient(const FString& RemoteIPv6, int32 LocalListenPort = 7777, int32 RemotePort = 7777);

	void Stop();

private:
	FSocket* IPv6Socket = nullptr;
	FSocket* IPv4Socket = nullptr;

	TSharedPtr<FInternetAddr> TargetIPv4Addr;
	TSharedPtr<FInternetAddr> TargetIPv6Addr;

	TSharedPtr<FInternetAddr> LastRemoteIPv6Addr;
	TSharedPtr<FInternetAddr> LastLocalIPv4Addr;

	FCriticalSection EndpointMutex;

	TFuture<void> TaskA;
	TFuture<void> TaskB;

	FThreadSafeBool bRunning = false;

	bool CreateAddress(const FString& Ip, int32 Port, TSharedPtr<FInternetAddr>& OutAddr);
	FSocket* CreateUdpSocket(const FString& Name, const FName& Protocol, const FString& BindIp, int32 BindPort, bool bIPv6Only);

	void ServerIPv6ToUE();
	void ServerUEToIPv6();

	void ClientUEToIPv6();
	void ClientIPv6ToUE();

	static FString NormalizeIPv6(const FString& InIPv6);
};
