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

#   可能遇到的问题
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

#  RandomDungeon插件的使用
这个插件实际上是多个的混合，包括了地图生成逻辑、敌人生成逻辑和物品生成逻辑，但目前设计的还比较简单打算在后面慢慢往里面添加新的逻辑
如果只需要把地牢、敌人和物品生成出来，只需要拖入BP_DungeonRuntimeManager，里面会依次调用地牢生成、敌人生成和物品生成。
如果想尝试最小的闭环可以用UE5自带的Variant_Shooter项目只需要通过在Lvl_Shooter很少的操作就能实现闭环(我之后会一步步把每个部分的实现说明白这里就是只关心最小的实现循环)

1.BP_ShooterCharacter加入BPI_DungeonPlayer接口我插件里敌人追踪和传送门的逻辑都是看有没有这个插件，如果有就能实现传送和敌人的追逐

<img width="1070" height="512" alt="image" src="https://github.com/user-attachments/assets/ded2e3ad-449b-415b-bcaf-30fb87993a80" />
<img width="442" height="106" alt="image" src="https://github.com/user-attachments/assets/697535cb-e1d2-43f2-ad78-08a2ebc6648e" />

2.往BP_ShooterCharacter加入新变量bIsDead（这个的主要目的是实现不会追已经死亡的角色），然后在事件图表里中的Even On Death中加入set bIsDead为true

<img width="597" height="529" alt="image" src="https://github.com/user-attachments/assets/1b826b0c-722d-4069-9d3b-61f797bb5cfe" />

3.点开左下角的接口会有一个CanBeTargeted函数，按照这么设置实现死亡后不会再追

<img width="479" height="202" alt="image" src="https://github.com/user-attachments/assets/25602490-4a70-487e-836c-584a58950484" />

4.调整地牢的起始生成位置和摧毁Z以及Nav的位置，要求摧毁Z不要涉及到地牢然后Nav要覆盖地牢（Nav是导航网格，是实现敌人追逐的基础）。理论上有人物攻击和敌人攻击的闭环，但因为一开始设计的时候这一块用了相通的名字所以不需要修改，直接就实现了。

<img width="1055" height="452" alt="image" src="https://github.com/user-attachments/assets/59d9537f-3c72-4e21-9b55-9a629b94fe40" />
<img width="962" height="430" alt="image" src="https://github.com/user-attachments/assets/b663142f-7286-46e6-b31f-bcc476b753b9" />

5.然后是实现物品的拾取然后放在身上
先去input里加入新的输入操作（放在外层或者shoot的input里都可，然后这里不用动了，里面不用管）

<img width="584" height="173" alt="image" src="https://github.com/user-attachments/assets/13dbb46a-be42-4de8-b71d-f35ed6011acc" />

在IMC_Default中加入输入操作并且绑定键位

<img width="774" height="405" alt="image" src="https://github.com/user-attachments/assets/d18a807b-f069-4174-8ad0-d8f58cc3b0f0" />

在BP_ShooterCharacter中加入BP_DungeonInventoryComponent组件，然后加入下面的方法，之后会调用我物品拾取那里的操作这里可以先不管

<img width="1163" height="504" alt="image" src="https://github.com/user-attachments/assets/54a1dac3-6fab-4044-8091-93656c2ede51" />

加入界面的初始化（注意目前这里的倒计时和总分还没搞要接下来完成）

<img width="437" height="163" alt="image" src="https://github.com/user-attachments/assets/1f27fb3a-8b51-4ffe-8341-f51a8966e6ed" />







