// Fill out your copyright notice in the Description page of Project Settings.


#include "Menu.h"
#include "Components/Button.h"
#include "MultiplayerSessionsSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "OnlineSubsystem.h"
#include "Components/CanvasPanel.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Misc/Base64.h"

// 只能通过window自带的方法来实现IPV6地址的获取
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iphlpapi.h>
#include "Windows/HideWindowsPlatformTypes.h"
#endif

void UMenu::MenuSetup(int32 NumberOfPublicConnections, FString TypeOfMatch, FString LobbyPath, FString InRoomCode, bool bInUseLAN)
{
	PathToLobby = FString::Printf(TEXT("%s?listen"), *LobbyPath);
	NumPublicConnections = NumberOfPublicConnections;
	MatchType = TypeOfMatch;
	RoomCode = InRoomCode;
	bUseLAN = bInUseLAN;
	AddToViewport();
	SetVisibility(ESlateVisibility::Visible);
	bIsFocusable = true;

	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController) {
			FInputModeUIOnly InputModeData;
			InputModeData.SetWidgetToFocus(TakeWidget());
			InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(true);
		}
	}

	UGameInstance* GameInstance = GetGameInstance();
	if (GameInstance) {
		MultiplayerSessionsSubsystem = GameInstance->GetSubsystem<UMultiplayerSessionsSubsystem>();
	}

	if (MultiplayerSessionsSubsystem)
	{
		MultiplayerSessionsSubsystem->MultiplayerOnCreateSessionComplete.AddDynamic(this, &ThisClass::OnCreateSession);
		MultiplayerSessionsSubsystem->MultiplayerOnFindSessionComplete.AddUObject(this, &ThisClass::OnFindSession);
		MultiplayerSessionsSubsystem->MultiplayerOnJoinSessionComplete.AddUObject(this, &ThisClass::OnJoinSession);
		MultiplayerSessionsSubsystem->MultiplayerOnDestroySessionComplete.AddDynamic(this, &ThisClass::OnDestroySession);
		MultiplayerSessionsSubsystem->MultiplayerOnStartSessionComplete.AddDynamic(this, &ThisClass::OnStartSession);
	}
}

bool UMenu::Initialize()
{
	if (!Super::Initialize()) {
		return false;
	}
	/*
	if (HostButton) {
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}
	if (JoinButton) {
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}
	*/
	if (HostButton)
	{
		HostButton->OnClicked.AddDynamic(this, &ThisClass::HostButtonClicked);
	}

	if (JoinButton)
	{
		JoinButton->OnClicked.AddDynamic(this, &ThisClass::JoinButtonClicked);
	}

	if (CreatePanel)
	{
		CreatePanel->SetVisibility(ESlateVisibility::Hidden);
	}

	if (JoinPanel)
	{
		JoinPanel->SetVisibility(ESlateVisibility::Hidden);
	}

	if (ConfirmCreateButton)
	{
		ConfirmCreateButton->OnClicked.AddDynamic(this, &ThisClass::ConfirmCreateButtonClicked);
	}

	if (ConfirmJoinButton)
	{
		ConfirmJoinButton->OnClicked.AddDynamic(this, &ThisClass::ConfirmJoinButtonClicked);
	}

	if (CopyButton)
	{
		CopyButton->OnClicked.AddDynamic(
			this,
			&ThisClass::CopyButtonClicked
		);
	}


	CurrentInviteCode = TEXT("");
	if (CreateRoomCodeText)
	{
		CreateRoomCodeText->SetText(FText::FromString(TEXT("")));
	}

	if (CreateModeBox)
	{
		CreateModeBox->OnSelectionChanged.AddDynamic(
			this,
			&ThisClass::CreateModeBoxSelectionChanged
		);
	}

	RefreshModeOptions();

	return true;
}

void UMenu::NativeDestruct()
{
	MenuTearDown();
	Super::NativeDestruct();
}

void UMenu::OnCreateSession(bool bWasSuccessful)
{
	if (bWasSuccessful)
	{
		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				15.f,
				FColor::Yellow,
				TEXT("Session created successfully")
			);
		}

		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(PathToLobby);
		}
		return;
	}

	if (ConfirmCreateButton)
	{
		ConfirmCreateButton->SetIsEnabled(true);
	}

	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Red,
			TEXT("CreateSession failed")
		);
	}
}

void UMenu::OnFindSession(const TArray<FOnlineSessionSearchResult>& SessionResults, bool bWasSuccessful)
{
	if (!MultiplayerSessionsSubsystem)
	{
		if (ConfirmJoinButton)
		{
			ConfirmJoinButton->SetIsEnabled(true);
		}
		GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Red, TEXT("No MultiplayerSessionsSubsystem"));
		return;
	}

	GEngine->AddOnScreenDebugMessage(
		-1, 20.f, FColor::Yellow,
		FString::Printf(TEXT("Menu OnFindSession: Success=%d Num=%d NeedMatchType=[%s]"),
			bWasSuccessful,
			SessionResults.Num(),
			*MatchType)
	);

	for (const auto& Result : SessionResults)
	{
		FString FoundMatchType;
		FString FoundRoomCode;


		const bool bHasMatchType = Result.Session.SessionSettings.Get(FName("MatchType"), FoundMatchType);
		const bool bHasRoomCode = Result.Session.SessionSettings.Get(FName("RoomCode"), FoundRoomCode);

		GEngine->AddOnScreenDebugMessage(
			-1,
			20.f,
			FColor::Green,
			FString::Printf(
				TEXT("Found MT=%s RC=%s"),
				*FoundMatchType,
				*FoundRoomCode
			)
		);

		if (bHasMatchType && bHasRoomCode && FoundMatchType == MatchType && FoundRoomCode == RoomCode)
		{
			MultiplayerSessionsSubsystem->JoinSession(Result);
			return;
		}
	}
	if (ConfirmJoinButton)
	{
		ConfirmJoinButton->SetIsEnabled(true);
	}
	GEngine->AddOnScreenDebugMessage(-1, 20.f, FColor::Red, TEXT("No matching RoomCode / MatchType found"));
}

void UMenu::OnJoinSession(EOnJoinSessionCompleteResult::Type Result)
{
	if (Result != EOnJoinSessionCompleteResult::Success)
	{
		if (ConfirmJoinButton)
		{
			ConfirmJoinButton->SetIsEnabled(true);
		}
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("JoinSession failed"));
		return;
	}

	IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get();
	if (!Subsystem) return;

	IOnlineSessionPtr SessionInterface = Subsystem->GetSessionInterface();
	if (!SessionInterface.IsValid()) return;

	FString Address;
	if (!SessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
	{
		if (ConfirmJoinButton)
		{
			ConfirmJoinButton->SetIsEnabled(true);
		}
		GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Red, TEXT("GetResolvedConnectString failed"));
		return;
	}

	GEngine->AddOnScreenDebugMessage(-1, 10.f, FColor::Yellow, FString::Printf(TEXT("Connect: %s"), *Address));

	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if (PlayerController)
	{
		PlayerController->ClientTravel(Address, TRAVEL_Absolute);
	}
}

void UMenu::OnDestroySession(bool bWasSuccessful)
{
}

void UMenu::OnStartSession(bool bWasSuccessful)
{
}



void UMenu::HostButtonClicked()
{
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Blue,
			TEXT("Host Button Clicked")
		);
	}

	if (CreatePanel)
	{
		CreatePanel->SetVisibility(ESlateVisibility::Visible);
	}

	if (JoinPanel)
	{
		JoinPanel->SetVisibility(ESlateVisibility::Hidden);
	}

	UpdateCreateInviteCode();
}

void UMenu::JoinButtonClicked()
{
	if (GEngine) {  // 输出在线子系统
		GEngine->AddOnScreenDebugMessage(
			-1,
			15.f,
			FColor::Blue,
			FString::Printf(TEXT("Join Button Clicked"))
		);
	}
	/*
	if (MultiplayerSessionsSubsystem) {
		MultiplayerSessionsSubsystem->FindSessions(10000,RoomCode,bUseLAN);
	}
	*/
	if (JoinPanel)
	{
		JoinPanel->SetVisibility(ESlateVisibility::Visible);
	}

	if (CreatePanel)
	{
		CreatePanel->SetVisibility(ESlateVisibility::Hidden);
	}
}

//加载当前OSS下可用方式
void UMenu::RefreshModeOptions()
{
	IOnlineSubsystem* OSS = IOnlineSubsystem::Get();

	const bool bSteamUsable =
		OSS &&
		OSS->GetSubsystemName() == FName("STEAM");

	if (CreateModeBox)
	{
		CreateModeBox->ClearOptions();

		if (bSteamUsable)
		{
			CreateModeBox->AddOption(TEXT("Steam"));
			CreateModeBox->SetSelectedOption(TEXT("Steam"));
		}
		else
		{
			CreateModeBox->AddOption(TEXT("LAN"));
			CreateModeBox->AddOption(TEXT("IPv6"));
			CreateModeBox->SetSelectedOption(TEXT("LAN"));
		}
	}

	if (JoinModeBox)
	{
		JoinModeBox->ClearOptions();

		if (bSteamUsable)
		{
			JoinModeBox->AddOption(TEXT("Steam"));
			JoinModeBox->SetSelectedOption(TEXT("Steam"));
		}
		else
		{
			JoinModeBox->AddOption(TEXT("LAN"));
			JoinModeBox->AddOption(TEXT("IPv6"));
			JoinModeBox->SetSelectedOption(TEXT("LAN"));
		}
	}
}

void UMenu::CopyButtonClicked()
{
	if (!CurrentInviteCode.IsEmpty())
	{
		FPlatformApplicationMisc::ClipboardCopy(*CurrentInviteCode);

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				-1,
				2.f,
				FColor::Green,
				TEXT("Invite code copied")
			);
		}
	}
}


void UMenu::ConfirmCreateButtonClicked()
{
	if (ConfirmCreateButton)
	{
		ConfirmCreateButton->SetIsEnabled(false);
	}
	const FString Mode = CreateModeBox ? CreateModeBox->GetSelectedOption() : TEXT("Steam");

	if (Mode == TEXT("Steam"))
	{
		if (MultiplayerSessionsSubsystem)
		{
			MultiplayerSessionsSubsystem->CreateSession(
				NumPublicConnections,
				MatchType,
				RoomCode,
				false
			);
		}
	}
	else if (Mode == TEXT("LAN"))
	{
		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(PathToLobby);
		}
	}
	else if (Mode == TEXT("IPv6"))
	{
		if (MultiplayerSessionsSubsystem)
		{
			MultiplayerSessionsSubsystem->StartIPv6ServerProxy();
		}

		if (UWorld* World = GetWorld())
		{
			World->ServerTravel(PathToLobby);
		}
	}
}


void UMenu::ConfirmJoinButtonClicked()
{
	if (ConfirmJoinButton)
	{
		ConfirmJoinButton->SetIsEnabled(false);
	}

	const FString Mode = JoinModeBox ? JoinModeBox->GetSelectedOption() : TEXT("Steam");

	if (Mode == TEXT("Steam"))
	{
		if (!JoinInputBox)
		{
			if (ConfirmJoinButton)
			{
				ConfirmJoinButton->SetIsEnabled(true);
			}
			return;
		}

		RoomCode = JoinInputBox->GetText().ToString().TrimStartAndEnd().ToUpper();

		if (RoomCode.IsEmpty())
		{
			if (ConfirmJoinButton)
			{
				ConfirmJoinButton->SetIsEnabled(true);
			}
			return;
		}

		if (MultiplayerSessionsSubsystem)
		{
			MultiplayerSessionsSubsystem->FindSessions(
				10000,
				RoomCode,
				false
			);
		}
	}
	else if (Mode == TEXT("LAN"))
	{
		if (!JoinInputBox)
		{
			if (ConfirmJoinButton)
			{
				ConfirmJoinButton->SetIsEnabled(true);
			}
			return;
		}

		const FString Code = JoinInputBox->GetText().ToString();
		const FString DecodedIp = DecodeInviteAddress(Code);
		const FString Address = AddDefaultPortIfNeeded(DecodedIp);

		APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
		if (PlayerController && !Address.IsEmpty())
		{
			PlayerController->ClientTravel(Address, TRAVEL_Absolute);
		}
		else
		{
			if (ConfirmJoinButton)
			{
				ConfirmJoinButton->SetIsEnabled(true);
			}
		}
	}
	else if (Mode == TEXT("IPv6"))
	{
		if (!JoinInputBox)
		{
			if (ConfirmJoinButton)
			{
				ConfirmJoinButton->SetIsEnabled(true);
			}
			return;
		}

		const FString RemoteIPv6 = DecodeInviteAddress(JoinInputBox->GetText().ToString());

		if (RemoteIPv6.IsEmpty())
		{
			if (ConfirmJoinButton)
			{
				ConfirmJoinButton->SetIsEnabled(true);
			}
			return;
		}

		if (MultiplayerSessionsSubsystem)
		{
			const bool bStarted = MultiplayerSessionsSubsystem->StartIPv6ClientProxy(RemoteIPv6);

			if (!bStarted)
			{
				if (ConfirmJoinButton)
				{
					ConfirmJoinButton->SetIsEnabled(true);
				}
				if (GEngine)
				{
					GEngine->AddOnScreenDebugMessage(
						-1,
						10.f,
						FColor::Red,
						TEXT("IPv6 proxy failed to start")
					);
				}
				return;
			}
		}

		APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
		if (PlayerController)
		{
			PlayerController->ClientTravel(TEXT("127.0.0.1:7777"), TRAVEL_Absolute);
		}
	}
}


void UMenu::MenuTearDown()
{
	RemoveFromParent();
	UWorld* World = GetWorld();
	if (World)
	{
		APlayerController* PlayerController = World->GetFirstPlayerController();
		if (PlayerController)
		{
			FInputModeGameOnly InputModeData;
			PlayerController->SetInputMode(InputModeData);
			PlayerController->SetShowMouseCursor(false);
		}
	}
}

FString UMenu::EncodeInviteAddress(const FString& Address) const
{
	return FBase64::Encode(Address);
}

FString UMenu::DecodeInviteAddress(const FString& Code) const
{
	FString Decoded;
	if (FBase64::Decode(Code, Decoded))
	{
		return Decoded;
	}

	return Code; // 允许玩家直接输入原始IP
}

FString UMenu::AddDefaultPortIfNeeded(const FString& Address) const
{
	if (Address.Contains(TEXT(":")) && !Address.Contains(TEXT(".")))
	{
		return Address; // IPv6 给代理用，不加端口
	}

	if (Address.Contains(TEXT(":")))
	{
		return Address; // IPv4:Port
	}

	return Address + TEXT(":7777");
}

FString UMenu::GetBestLocalIPv4() const
{
	ISocketSubsystem* SocketSubsystem = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
	if (!SocketSubsystem) return TEXT("");

	TArray<TSharedPtr<FInternetAddr>> Addresses;
	SocketSubsystem->GetLocalAdapterAddresses(Addresses);

	for (const TSharedPtr<FInternetAddr>& Addr : Addresses)
	{
		if (!Addr.IsValid()) continue;

		const FString Ip = Addr->ToString(false);

		if (Ip.Contains(TEXT(".")) &&
			!Ip.StartsWith(TEXT("127.")) &&
			!Ip.StartsWith(TEXT("169.254")) &&
			Ip != TEXT("0.0.0.0"))
		{
			return Ip;
		}
	}

	return TEXT("");
}

FString UMenu::GetBestLocalIPv6() const
{
#if PLATFORM_WINDOWS
	ULONG BufferSize = 15000;
	TArray<uint8> Buffer;
	Buffer.SetNum(BufferSize);

	IP_ADAPTER_ADDRESSES* Addresses =
		reinterpret_cast<IP_ADAPTER_ADDRESSES*>(Buffer.GetData());

	ULONG Result = GetAdaptersAddresses(
		AF_INET6,
		GAA_FLAG_SKIP_ANYCAST |
		GAA_FLAG_SKIP_MULTICAST |
		GAA_FLAG_SKIP_DNS_SERVER,
		nullptr,
		Addresses,
		&BufferSize
	);

	if (Result == ERROR_BUFFER_OVERFLOW)
	{
		Buffer.SetNum(BufferSize);
		Addresses =
			reinterpret_cast<IP_ADAPTER_ADDRESSES*>(Buffer.GetData());

		Result = GetAdaptersAddresses(
			AF_INET6,
			GAA_FLAG_SKIP_ANYCAST |
			GAA_FLAG_SKIP_MULTICAST |
			GAA_FLAG_SKIP_DNS_SERVER,
			nullptr,
			Addresses,
			&BufferSize
		);
	}

	if (Result != NO_ERROR)
	{
		UE_LOG(LogTemp, Warning, TEXT("GetAdaptersAddresses failed: %lu"), Result);
		return TEXT("");
	}

	for (IP_ADAPTER_ADDRESSES* Adapter = Addresses; Adapter != nullptr; Adapter = Adapter->Next)
	{
		if (Adapter->OperStatus != IfOperStatusUp)
		{
			continue;
		}

		for (IP_ADAPTER_UNICAST_ADDRESS* Unicast = Adapter->FirstUnicastAddress;
			Unicast != nullptr;
			Unicast = Unicast->Next)
		{
			if (!Unicast->Address.lpSockaddr)
			{
				continue;
			}

			if (Unicast->Address.lpSockaddr->sa_family != AF_INET6)
			{
				continue;
			}

			sockaddr_in6* Addr6 =
				reinterpret_cast<sockaddr_in6*>(Unicast->Address.lpSockaddr);

			wchar_t IpBuffer[INET6_ADDRSTRLEN] = {};

			if (!InetNtopW(AF_INET6, &Addr6->sin6_addr, IpBuffer, INET6_ADDRSTRLEN))
			{
				continue;
			}

			FString Ip = FString(IpBuffer);

			UE_LOG(LogTemp, Warning, TEXT("Windows IPv6 Candidate: %s"), *Ip);

			if (Ip == TEXT("::1") ||
				Ip == TEXT("::") ||
				Ip.StartsWith(TEXT("fe80"), ESearchCase::IgnoreCase))
			{
				continue;
			}

			if (Ip.StartsWith(TEXT("2")) || Ip.StartsWith(TEXT("3")))
			{
				UE_LOG(LogTemp, Warning, TEXT("Selected Windows IPv6: %s"), *Ip);
				return Ip;
			}
		}
	}
#endif

	return TEXT("");
}

//IPV6和LAN选择后改邀请码

void UMenu::CreateModeBoxSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	UpdateCreateInviteCode();
}

void UMenu::UpdateCreateInviteCode()
{
	const FString Mode = CreateModeBox ? CreateModeBox->GetSelectedOption() : TEXT("Steam");

	if (Mode == TEXT("Steam"))
	{
		CurrentInviteCode = FGuid::NewGuid()
			.ToString(EGuidFormats::Digits)
			.Left(8)
			.ToUpper();

		RoomCode = CurrentInviteCode;
	}
	else if (Mode == TEXT("LAN"))
	{
		const FString LocalIPv4 = GetBestLocalIPv4();
		CurrentInviteCode = LocalIPv4.IsEmpty()
			? TEXT("NO_LAN_IP")
			: EncodeInviteAddress(LocalIPv4);
	}
	else if (Mode == TEXT("IPv6"))
	{
		const FString LocalIPv6 = GetBestLocalIPv6();
		CurrentInviteCode = LocalIPv6.IsEmpty()
			? TEXT("NO_IPV6")
			: EncodeInviteAddress(LocalIPv6);
	}

	if (CreateRoomCodeText)
	{
		CreateRoomCodeText->SetText(FText::FromString(CurrentInviteCode));
	}
}
