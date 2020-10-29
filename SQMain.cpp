#include "SQMain.h"
#include "SQConsts.h"
#include "SQFuncs.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
PluginFuncs* VCMP;
HSQUIRRELVM v;
HSQAPI sq;
extern int PORT;
// Attaching plugin's load to the squirrel VM
void LoadSquirrelScript() {
	// See if we have any imports from Squirrel
	size_t size;
	int32_t sqId      = VCMP->FindPlugin(const_cast<char*>("SQHost2"));
	const void ** sqExports = VCMP->GetPluginExports(sqId, &size);

	// We do!
	if (sqExports != NULL && size > 0) {
		// Cast to a SquirrelImports structure
		SquirrelImports ** sqDerefFuncs = (SquirrelImports **)sqExports;
		
		// Now let's change that to a SquirrelImports pointer
		SquirrelImports * sqFuncs       = (SquirrelImports *)(*sqDerefFuncs);
		// Now we get the virtual machine
		if (sqFuncs) {
			// Get a pointer to the VM and API
			v = *(sqFuncs->GetSquirrelVM());
			sq = *(sqFuncs->GetSquirrelAPI());

			// Register functions
			RegisterMyFuncs(v);
			
			// Register constants
			RegisterMyConsts(v);
		}
	}
	else
		OutputError("Failed to attach to SQHost2.");
}

// Called when server is loading the plugin
uint8_t OnServerInitialise() {
	ServerSettings c;
	VCMP->GetServerSettings(&c);
	if (c.port != PORT)
	{
		printf("Setting port to %d\n", c.port);
		PORT = c.port;
	}
	OutputMessage("Loaded actorplus module..");
	return 1;
}

// Called when the server is unloading the plugin
void OnServerShutdown() {
   
}

// Called when the server is loading the Squirrel plugin
uint8_t OnPluginCommand(uint32_t type, const char* text) {
	switch (type) {
		case 0x7D6E22D8:
			LoadSquirrelScript();
			//OnSquirrelScriptLoad();
			break;
		default:
			break;
	}

	return 1;
}
void onPlayerDeath(int32_t playerid, int32_t killerid, int32_t reason,
	vcmpBodyPart)
{
	if (IsPlayerActor(playerid) == 1)
	{
		int aid = GetActorID(playerid);
		if (aid != -1)
		{
			int top = sq->gettop(v);
			sq->pushroottable(v);
			sq->pushstring(v, _SC("onActorDeath"), -1);
			if (SQ_SUCCEEDED(sq->get(v, -2))) {
				sq->pushroottable(v);
				sq->pushinteger(v, aid);
				sq->call(v, 2, 0, 0);
			}
			sq->settop(v, top);
		}
	}
}
void onPlayerSpawn(int32_t playerid)
{
	if (IsPlayerActor(playerid))
	{
		int aid = GetActorID(playerid);
		if (aid != -1)
		{
			int skinId = GetActorSkin(aid);
			if (skinId != -1)
			{
				VCMP->SetPlayerSkin(playerid, skinId);
			}
		}
	}
}

void onPlayerUpdate(int32_t playerid, vcmpPlayerUpdate u)
{
	//u.
}
extern "C" unsigned int VcmpPluginInit(PluginFuncs* pluginFuncs, PluginCallbacks* pluginCalls, PluginInfo* pluginInfo) {
	VCMP = pluginFuncs;
	
	// Plugin information
	pluginInfo->pluginVersion = 0x110;
	pluginInfo->apiMajorVersion = PLUGIN_API_MAJOR;
	pluginInfo->apiMinorVersion = PLUGIN_API_MINOR;
	strcpy(pluginInfo->name, "actor");
	pluginCalls->OnServerInitialise = OnServerInitialise;
	pluginCalls->OnServerShutdown = OnServerShutdown;
	pluginCalls->OnPluginCommand = OnPluginCommand;
	pluginCalls->OnPlayerSpawn = onPlayerSpawn;
	pluginCalls->OnPlayerDeath = onPlayerDeath;
	
	return 1;
}

