// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Menu.generated.h"

/**
 * 
 */
UCLASS()
class MULTIPLAYERSESSIONS_API UMenu : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void MenuSetup(int32 NumberOfPublicConnections = 4, FString TypeOfMatch = FString(TEXT("ContractRun")),FString LobbyPath = FString(TEXT("")), FString InRoomCode = FString(TEXT("12345")), bool bInUseLAN = false);

protected:
	virtual bool Initialize() override;
	virtual void NativeDestruct() override;

	//菜单类自定义委托的回调函数，多人回调系统
	UFUNCTION()
	void OnCreateSession(bool bWasSuccessful);
	void OnFindSession(const TArray<FOnlineSessionSearchResult>& SessoinResult, bool bWasSuccessful);
	void OnJoinSession(EOnJoinSessionCompleteResult::Type Result);
	UFUNCTION()
	void OnDestroySession(bool bWasSuccessful);
	UFUNCTION()
	void OnStartSession(bool bWasSuccessful);

private:

	
	UPROPERTY(meta = (BindWidget))
	class UButton* HostButton;

	UPROPERTY(meta = (BindWidget))
	UButton* JoinButton;

	UPROPERTY(meta = (BindWidget))
	class UCanvasPanel* CreatePanel;

	UPROPERTY(meta = (BindWidget))
	UCanvasPanel* JoinPanel;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmCreateButton;

	UPROPERTY(meta = (BindWidget))
	UButton* ConfirmJoinButton;

	UPROPERTY(meta = (BindWidget))
	class UComboBoxString* CreateModeBox;

	UPROPERTY(meta = (BindWidget))
	UComboBoxString* JoinModeBox;

	UPROPERTY(meta = (BindWidget))
	class UEditableTextBox* JoinInputBox;

	UPROPERTY(meta = (BindWidget))
	UButton* CopyButton;

	UPROPERTY(meta = (BindWidget))
	UTextBlock* CreateRoomCodeText;

	UFUNCTION()
	void CopyButtonClicked();

	void RefreshModeOptions();

	UFUNCTION()
	void ConfirmCreateButtonClicked();

	UFUNCTION()
	void ConfirmJoinButtonClicked();

	UFUNCTION()
	void HostButtonClicked();

	UFUNCTION()
	void JoinButtonClicked();

	void MenuTearDown();

	// 统一邀请码的生成逻辑
	UFUNCTION()
	void CreateModeBoxSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

	void UpdateCreateInviteCode();


	// 负责处理所有在线会话功能
	class UMultiplayerSessionsSubsystem* MultiplayerSessionsSubsystem;

	int32 NumPublicConnections{ 4 };
	FString MatchType{ TEXT("ContractRun") };
	FString PathToLobby{TEXT("")};
	FString RoomCode{ TEXT("12345") };
	bool bUseLAN{ false };

	//界面上显示的邀请码，steam是一个房间号，然后LAN和IPV6则是IP地址的加密版
	FString CurrentInviteCode;

	// 用来获取IPV4和IPV6的地址然后用Base64加密
	FString GetBestLocalIPv4() const;
	FString GetBestLocalIPv6() const;
	FString EncodeInviteAddress(const FString& Address) const;
	FString DecodeInviteAddress(const FString& Code) const;
	FString AddDefaultPortIfNeeded(const FString& Address) const;
};
