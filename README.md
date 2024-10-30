# Faldon

Faldon is a free online role-playing game. You can explore the world, fight monsters, craft items, and interact with other players. Faldon does not limit a player's abilities and skills with class restrictions.
Adventure awaits in the many towns, dungeons, deserts and forests of Faldon. Compete for rank on the Top 20 Ladder, form guilds and sell on the market.
Level up, raise skills, learn spells. There's always much to do. You can also play soccer or become a peace-loving alchemist, chef or blacksmith. You have the freedom to develop your character the way you want to.
https://www.faldonrpg.com/

This code is a packet sender for Faldon, allowing you to create afk-leveling scripts by simply adding desired monster attacking packets to it as well as teleport spell packets to teleport around monsters spawns and not get caught by The Watcher (Faldon anti afk-leveling system).

The goal is having the monster entity list then calculate the distance between local player and monsters and send their respective attacking packets only if they are close enough to be attacked, this way packets won't be sent all the time and endlessly, although I couldn't find the monster entity list. While I'm still looking for it, I've thought of trying to detect if the monster is alive by hooking WSARecv function and check if client received monster spawning packet, or a better way would be to create a MobSelect function which selects mobs in your LOS (line of sight) then send packets for that selected mob only.
(To be honest, the goal was to create a new Maldon, with overflows and crashes fixed. Maldon (composed by MLoader.exe + Maldon.dll) attaches to client.exe and once you start it by typing ~auto on in chat, it will send monster attacking packets whenever you get close to it)

After 2 months using and improving the Packet Sender to create afk scripts (mainly afk-leveling), I learned a little about Hooking and got to hook WSARecv and WSASend functions as well as OpenGL to build a GUI (menu) containing all features I've coded so far. By hooking those functions, it was possible to build an Autotool.dll achieving my goal plus a lot of features which I was not expecting to build, I called it Auto-Tool. I had to build a DLL injector to inject the Auto-Tool.dll too, so I wouldn't have to mess with any dll injector that could be unsafe.
