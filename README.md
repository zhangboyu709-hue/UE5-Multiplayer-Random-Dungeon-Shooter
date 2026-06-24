# UE5-Multiplayer-Random-Dungeon-Shooter
这个项目主要还是两个插件，一个插件支持steam、LAN和IPV6多种连接，另一个插件主要是随机地图生成的简单版本，支持随机生成地图、敌人和物品，然后有一个简单的背包系统和得分。
#  MultiplayerSessions插件的使用方法
1.删除Binaries和Intermediate，然后直接复制进项目里，在UE5的插件中加入steam sockets和Online Subsystem Steam
<img width="363" height="275" alt="image" src="https://github.com/user-attachments/assets/8a16d2a3-79cd-4519-af9d-19e872ecf9fb" />

2.在DefaultEngine中加入下面（这个只有steam方法才需要，然后目前我没找到如何在项目启动的时候实现SteamSocketsNetDriver和IpNetDriver的转换，启动后就只支持其中一种，然后优先steam）
```ini
[/Script/Engine.GameEngine]
!NetDriverDefinitions=ClearArray
+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="/Script/SteamSockets.SteamSocketsNetDriver",DriverClassNameFallback="OnlineSubsystemUtils.IpNetDriver")

[OnlineSubsystem]
DefaultPlatformService=Steam

[OnlineSubsystemSteam]
bEnabled=true
SteamDevAppId=480
bInitServerOnClient=true

[/Script/OnlineSubsystemSteam.SteamNetDriver]
NetConnectionClassName="/Script/SteamSockets.SteamSocketsNetConnection"
```
3.点开关卡蓝图，开始事件连出来一个create widget,选中WBP Menu连接起来。Type of Match和Lobby path看你个人项目设置。这样就能实现使用了。

<img width="865" height="442" alt="image" src="https://github.com/user-attachments/assets/022b55c6-4e3b-4ddc-b54d-8a2d174d3cda" />

Widget要用这个，然后从Return Value中引出来。

<img width="301" height="85" alt="image" src="https://github.com/user-attachments/assets/9a118966-ef1c-42ef-bd5b-220757a10502" />

4.目前我只做了创建前复制邀请码，如果如果需要看邀请码的信息用我下面的第一个截图，如果要复制用第二个

<img width="608" height="272" alt="image" src="https://github.com/user-attachments/assets/0fa48e89-c239-4f13-aae1-a599a54cb928" />
<img width="544" height="209" alt="image" src="https://github.com/user-attachments/assets/2bd54a4d-2a51-4098-a05e-b9ef8b53110b" />

#  可能遇到的问题
1.steam没法连接，创建后找不到，可以先看看两台电脑的steam的设置里的下载服务器是不是同一个，不是同一个连不上。

<img width="627" height="326" alt="image" src="https://github.com/user-attachments/assets/32db261f-8bc2-444d-a8c6-c57c6ba1ef56" />

2.用LAN和IPV6的时候发生连接不了，可以看看你的电脑是不是专用网络，公用网络不能作为发起者。

<img width="661" height="179" alt="image" src="https://github.com/user-attachments/assets/5676b1c1-f63c-4b21-917e-bb9b45ce0943" />

3.如果在编辑器里能用但是打包后没法用，建议先看看地图有没有加入打包中

<img width="491" height="229" alt="image" src="https://github.com/user-attachments/assets/80737ff4-3bbe-4f78-b457-9bcb87cef64b" />

如果想要省事（之后可能加入大量地图，并且不会频繁更换项目），可以考虑把文件夹放入打包中

<img width="865" height="373" alt="image" src="https://github.com/user-attachments/assets/2151d5cb-5410-47ab-b3e9-121c9e9383e4" />

如果还是没法打包可以去config下的DefaultGame.ini中看看是不是CookRule=Unknown，然后改成AlwaysCook

<img width="865" height="56" alt="image" src="https://github.com/user-attachments/assets/cec6de14-9384-477b-8442-2b8fee7d10ac" />

#  待补全
