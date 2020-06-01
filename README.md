# linux-actor ( Plugin for Vice City Multiplayer Game )

https://forum.vc-mp.org/?topic=7741.msg48548#msg48548

Installation
Place the actor64.so/ actor32.so in plugins folder.
Run Server

Use ysc's Console Input Plugin also. https://forum.vc-mp.org/?topic=387.msg2416#msg2416

Now in console type, 

create_actor("john") will create a bot in your server

create_actor("tommy",0 ) will create Tommy Vercetti in server ( skin id of tommy == 0 )

create_actor("cop",1,-656,756,11.2,0) will create a cop in downtown. ( 0 == angle ).

set_actor_angle(  actorId, angle ),            angle= -3.14 to 3.14

If port is different than 8192, then set_port(8193) if it is 8193

If you have any trouble, see this https://forum.vc-mp.org/?topic=7741.msg48266#msg48266 (although it is old )

THanks. (btw the VCMP plugin structure was not known to the author in the beginning. So he used another person's another plugin structure as 
base for developing this plugin (GeoIP by Crys) 
