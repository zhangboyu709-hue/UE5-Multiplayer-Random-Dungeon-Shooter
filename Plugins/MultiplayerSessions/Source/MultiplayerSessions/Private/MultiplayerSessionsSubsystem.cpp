// Fill out your copyright notice in the Description page of Project Settings.


#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"

UMultiplayerSessionsSubsystem::UMultiplayerSessionsSubsystem() :
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	// 初始化 Session 创建完成委托，并绑定回调函数 OnCreateSessionConplete
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete)),
	DestroySessionCompleteDelegate(FOnDestroySessionCompleteDelegate::CreateUObject(this, &ThisClass::OnDestroySessionComplete)),
	StartSessionCompleteDelegate(FOnStartSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnStartSessionComplete))

{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem) {
		SessionInterface = Subsystem->GetSessionInterface();
	}
}

void UMultiplayerSessionsSubsystem::CreateSession(int32 NumPublicConnections, FString MatchType, FString RoomCode, bool bUseLAN)
{
	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (Subsystem)
	{
		SessionInterface = Subsystem->GetSessionInterface();

		GEngine->AddOnScreenDebugMessage(
			-1, 20.f, FColor::Green,
			FString::Printf(TEXT("Create runtime OSS=%s"), *Subsystem->GetSubsystemName().ToString())
		);
	}

	if (!SessionInterface.IsValid())
	{
		return;
	}
	//调用网络接口实现会话
	if (!SessionInterface.IsValid()) { // 判断在线会话接口有没有效
		return;
	}
	auto ExistingSession = SessionInterface->GetNamedSession(NAME_GameSession); // 获取在线的会话
	if (ExistingSession != nullptr) {     // 如果已经存在了就销毁
		bCreateSessionOnDestroy = true;
		LastNumPblicConnetctions = NumPublicConnections;
		LastMatchType = MatchType;
		LastRoomCode = RoomCode;
		bLastUseLAN = bUseLAN;
		DestroySession();
		return;
	}
	// 把委托加入委托句柄，方便之后移除
	CreateSessionCompleteDelegateHandle = SessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	LastSessionSettings = MakeShareable(new FOnlineSessionSettings());
	LastSessionSettings->bIsLANMatch = bUseLAN;
	LastSessionSettings->NumPublicConnections = NumPublicConnections;
	LastSessionSettings->bAllowJoinInProgress = true;
	LastSessionSettings->bAllowJoinViaPresence = true;
	LastSessionSettings->bShouldAdvertise = true;
	LastSessionSettings->bUsesPresence = true;
	LastSessionSettings->Set(FName("MatchType"), MatchType, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	LastSessionSettings->Set(FName("RoomCode"), RoomCode, EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);
	// Steam / UE5 很关键
	LastSessionSettings->bUseLobbiesIfAvailable = true; // UE5 在 Steam 上优先使用 Steam Lobby 系统来创建房间。
	LastSessionSettings->BuildUniqueId = 1;

	GEngine->AddOnScreenDebugMessage(
		-1, 20.f, FColor::Green,
		FString::Printf(TEXT("Create: OSS=%s LAN=%d MatchType=%s"),
			*IOnlineSubsystem::Get()->GetSubsystemName().ToString(),
			LastSessionSettings->bIsLANMatch,
			*MatchType)
	);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *LastSessionSettings)) {
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle); // 创建失败就通过这个移除

		// 广播自定义回调
		MultiplayerOnCreateSessionComplete.Broadcast(false);
	}
}

void UMultiplayerSessionsSubsystem::FindSessions(int32 MaxSearchResults, FString RoomCode, bool bUseLAN)
{
	if (!SessionInterface.IsValid()) {
		return;
	}

	FindSessionsCompleteDelegateHandle = SessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	LastSessionSearch = MakeShareable(new FOnlineSessionSearch());
	LastSessionSearch->MaxSearchResults = MaxSearchResults; // 搜索的最大值
	LastSessionSearch->bIsLanQuery = bUseLAN;  // 不用局域网
	LastSessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

	const FString NormalizedRoomCode = RoomCode.TrimStartAndEnd().ToUpper();
	LastSessionSearch->QuerySettings.Set(
		FName(TEXT("RoomCode")),
		NormalizedRoomCode,
		EOnlineComparisonOp::Equals
	);
	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), LastSessionSearch.ToSharedRef()))
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle); // 创建失败就通过这个移除

		// 广播自定义回调
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
	}

}

void UMultiplayerSessionsSubsystem::JoinSession(const FOnlineSessionSearchResult& SessionResult)
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
		return;
	}

	JoinSessionCompleteDelegateHandle = SessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	if (!SessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, SessionResult))
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);


		MultiplayerOnJoinSessionComplete.Broadcast(EOnJoinSessionCompleteResult::UnknownError);
	}

}

void UMultiplayerSessionsSubsystem::DestroySession()
{
	if (!SessionInterface.IsValid())
	{
		MultiplayerOnDestroySessionComplete.Broadcast(false);
		return;
	}

	DestroySessionCompleteDelegateHandle = SessionInterface->AddOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegate);

	if (!SessionInterface->DestroySession(NAME_GameSession))
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
		MultiplayerOnDestroySessionComplete.Broadcast(false);
	}

}

void UMultiplayerSessionsSubsystem::StartSession()
{

}

// IPV4和IPV6的转换
bool UMultiplayerSessionsSubsystem::StartIPv6ServerProxy()
{
	if (!IPv6Proxy)
	{
		IPv6Proxy = MakeUnique<FIPv6Proxy>();
	}

	return IPv6Proxy->StartServer(7777, 7777);
}

bool UMultiplayerSessionsSubsystem::StartIPv6ClientProxy(const FString& RemoteIPv6)
{
	if (!IPv6Proxy)
	{
		IPv6Proxy = MakeUnique<FIPv6Proxy>();
	}

	return IPv6Proxy->StartClient(RemoteIPv6, 7777, 7777);
}

void UMultiplayerSessionsSubsystem::StopIPv6Proxy()
{
	if (IPv6Proxy)
	{
		IPv6Proxy->Stop();
	}
}


void UMultiplayerSessionsSubsystem::OnCreateSessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegateHandle);
	}

	MultiplayerOnCreateSessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnFindSessionsComplete(bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegateHandle);
	}
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Yellow,
			FString::Printf(
				TEXT("Search Num=%d Success=%d"),
				LastSessionSearch->SearchResults.Num(),
				bWasSuccessful
			)
		);
	}
	if (LastSessionSearch->SearchResults.Num() <= 0)
	{
		MultiplayerOnFindSessionComplete.Broadcast(TArray<FOnlineSessionSearchResult>(), false);
		return;
	}

	MultiplayerOnFindSessionComplete.Broadcast(LastSessionSearch->SearchResults, bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegateHandle);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			20.f,
			FColor::Yellow,
			FString::Printf(TEXT("OnJoinSessionComplete Result=%d"), (int32)Result)
		);
	}

	MultiplayerOnJoinSessionComplete.Broadcast(Result);
}

void UMultiplayerSessionsSubsystem::OnDestroySessionComplete(FName SessionName, bool bWasSuccessful)
{
	if (SessionInterface)
	{
		SessionInterface->ClearOnDestroySessionCompleteDelegate_Handle(DestroySessionCompleteDelegateHandle);
	}
	if (bWasSuccessful && bCreateSessionOnDestroy)
	{
		bCreateSessionOnDestroy = false;
		CreateSession(LastNumPblicConnetctions, LastMatchType, LastRoomCode, bLastUseLAN);
	}
	MultiplayerOnDestroySessionComplete.Broadcast(bWasSuccessful);
}

void UMultiplayerSessionsSubsystem::OnStartSessionComplete(FName SessionName, bool bWasSuccessful)
{
}