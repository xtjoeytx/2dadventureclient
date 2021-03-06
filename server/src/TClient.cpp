#include "IDebug.h"
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

#include "TClient.h"
#include "main.h"
#include "TPlayer.h"
#include "TWeapon.h"
#include "IUtil.h"
#include "IConfig.h"
#include "TNPC.h"
#include "TMap.h"
#include "TLevel.h"
#include "TGameWindow.h"

static const char* const filesystemTypes[] =
{
	"all",
	"file",
	"level",
	"head",
	"body",
	"sword",
	"shield",
	nullptr
};

#if SDL_BYTEORDER == SDL_BIG_ENDIAN
static const Uint32 rmask = 0xff000000;
static const Uint32 gmask = 0x00ff0000;
static const Uint32 bmask = 0x0000ff00;
static const Uint32 amask = 0x000000ff;
#else
#endif

TClient::TClient(CString pName)
	: running(false), doRestart(false), name(pName), wordFilter(this), gameWindow(new TGameWindow(this))
#ifdef V8NPCSERVER
	, mScriptEngine(this), mPmHandlerNpc(nullptr)
#endif
#ifdef UPNP
	, upnp(this)
#endif
{
	auto time_now = std::chrono::high_resolution_clock::now();
	lastTimer = lastNWTimer = last1mTimer = last5mTimer = last3mTimer = time_now;
#ifdef V8NPCSERVER
	lastScriptTimer = time_now;
	accumulator = std::chrono::nanoseconds(0);
#endif

	atexit(SDL_Quit);

	// This has the full path to the server directory.
	runnerPath = CString() << getHomePath() << "data/"; // << "servers/" << name << "/";
	CFileSystem::fixPathSeparators(&runnerPath);

	// Set up the log files.
	CString logPath = CString() << getHomePath();
	CString npcLogPath = CString() << logPath << "logs/npclog.txt";
	CString rcLogPath = CString() << logPath << "logs/rclog.txt";
	CString clientLogPath = CString() << logPath << "logs/clientlog.txt";
	CFileSystem::fixPathSeparators(&npcLogPath);
	CFileSystem::fixPathSeparators(&rcLogPath);
	CFileSystem::fixPathSeparators(&clientLogPath);
	npclog.setFilename(npcLogPath);
	rclog.setFilename(rcLogPath);
	clientLog.setFilename(clientLogPath);

#ifdef V8NPCSERVER
	CString scriptPath = CString() << logPath << "logs/scriptlog.txt";
	CFileSystem::fixPathSeparators(&scriptPath);
	scriptlog.setFilename(scriptPath);
#endif

#ifndef __AMIGA__
	// Announce ourself to other classes.
	serverlist.setServer(this);
#endif
	for (auto & i : filesystem)
		i.setServer(this);
	filesystem_accounts.setServer(this);
}

TClient::~TClient()
{
	cleanup();
}

int TClient::init(
#ifndef __AMIGA__
		const CString& serverip, const CString& serverport, const CString& localip, const CString& serverinterface
#endif
)
{
	// Player ids 0 and 1 break things.  NPC id 0 breaks things.
	// Don't allow anything to have one of those ids.
	// Player ids 16000 and up is used for players on other servers and "IRC"-channels.
	// The players from other servers should be unique lists for each player as they are fetched depending on
	// what the player chooses to see (buddies, "global guilds" tab, "other servers" tab)
	playerIds.resize(2);
	npcIds.resize(10001); // Starting npc ids at 10,000 for now on..


#ifdef V8NPCSERVER
	// Initialize the Script Engine
	if (!mScriptEngine.Initialize())
	{
		clientLog.out("[%s] ** [Error] Could not initialize script engine.\n");
		// TODO(joey): new error number? log is probably enough
		return ERR_SETTINGS;
	}
#endif

	// Load the config files.
	int ret = loadConfigFiles();
	if (ret) return ret;

	setUpLocalPlayer();

	gameWindow->init();

	clientLog.out("Loading images:\n");

	for (auto & file : *filesystem[0].getFileList()) {
		if (file.first.find(".gif") > 0 || file.first.find(".png") > 0 || file.first.find(".jpg") > 0) {
			clientLog.out("\t%s\n", file.first.text());
			gameWindow->renderClear();
			gameWindow->drawText(gameWindow->font, file.first.text(), 10,10, { 255, 255, 255});
			gameWindow->renderPresent();

			TImage::find(file.first.text(), this, true);
		}
	}

	clientLog.out("Loading animations:\n");

	for (auto & file : *filesystem[0].getFileList()) {
		if (file.first.find(".gani") > 0) {
			clientLog.out("\t%s\n", file.first.text());
			gameWindow->renderClear();
			gameWindow->drawText(gameWindow->font, file.first.text(), 10,10, { 255, 255, 255});
			gameWindow->renderPresent();

			TAnimation::find(file.first.text(), this, true);
		}
	}

	clientLog.out("Loading sounds:\n");

	for (auto & file : *filesystem[0].getFileList()) {
		if (file.first.find(".wav") > 0) {
			clientLog.out("\t%s\n", file.first.text());
			gameWindow->renderClear();
			gameWindow->drawText(gameWindow->font, file.first.text(), 10, 10, { 255, 255, 255 });
			gameWindow->renderPresent();

			TSound::find(file.first.text(), this);
		}
	}

#ifndef __AMIGA__
	// If an override serverip and serverport were specified, fix the options now.
	if (!serverip.isEmpty())
		settings.addKey("serverip", serverip);
	if (!serverport.isEmpty())
		settings.addKey("serverport", serverport);
	if (!localip.isEmpty())
		settings.addKey("localip", localip);
	if (!serverinterface.isEmpty())
		settings.addKey("serverinterface", serverinterface);
	overrideIP = serverip;
	overridePort = serverport;
	overrideLocalIP = localip;
	overrideInterface = serverinterface;

	// Fix up the interface to work properly with CSocket.
	CString oInter = overrideInterface;
	if (overrideInterface.isEmpty())
		oInter = settings.getStr("serverinterface");
	if (oInter == "AUTO")
		oInter.clear();

	// Initialize the player socket.
	playerSock.setType(SOCKET_TYPE_SERVER);
	playerSock.setProtocol(SOCKET_PROTOCOL_TCP);
	playerSock.setDescription("playerSock");

	// Start listening on the player socket.
	serverlog.out("[%s]      Initializing player listen socket.\n", name.text());
	if (playerSock.init((oInter.isEmpty() ? nullptr : oInter.text()), settings.getStr("serverport").text()))
	{
		serverlog.out("[%s] ** [Error] Could not initialize listening socket...\n", name.text());
		return ERR_LISTEN;
	}
	if (playerSock.connect())
	{
		serverlog.out("[%s] ** [Error] Could not connect listening socket...\n", name.text());
		return ERR_LISTEN;
	}
#endif

#ifdef UPNP
	// Start a UPNP thread.  It will try to set a UPNP port forward in the background.
	serverlog.out("[%s]      Starting UPnP discovery thread.\n", name.text());
	upnp.initialize((oInter.isEmpty() ? playerSock.getLocalIp() : oInter.text()), settings.getStr("serverport").text());
	upnp_thread = std::thread(std::ref(upnp));
#endif

#ifdef V8NPCSERVER
	// Setup NPC Control port
	mNCPort = strtoint(settings.getStr("serverport"));

	mNpcServer = new TPlayer(this, nullptr, 0);
	mNpcServer->setType(PLTYPE_NPCSERVER);
	mNpcServer->loadAccount("(npcserver)");
	mNpcServer->setHeadImage(settings.getStr("staffhead", "head25.png"));
	mNpcServer->setLoaded(true);	// can't guarantee this, so forcing it


	// TODO(joey): Update this when server options is changed?
	// Set nickname, and append (Server) - required!
	CString nickName = settings.getStr("nickname");
	if (nickName.isEmpty())
		nickName = "NPC-Server";
	nickName << " (Server)";
	mNpcServer->setNick(nickName, true);

	// Add npc-server to playerlist
	addPlayer(mNpcServer);
#endif


#ifndef __AMIGA__
	// Connect to the serverlist.
	serverlog.out("[%s]      Initializing serverlist socket.\n", name.text());
	if (!serverlist.init(settings.getStr("listip"), settings.getStr("listport")))
	{
		serverlog.out("[%s] ** [Error] Could not initialize serverlist socket.\n", name.text());
		return ERR_LISTEN;
	}
	serverlist.connectServer();

	// Register ourself with the socket manager.
	sockManager.registerSocket((CSocketStub*)this);
	sockManager.registerSocket((CSocketStub*)&serverlist);
#endif


	return 0;
}

void TClient::log(const CString& format, ...) {
	va_list s_format_v;

	va_start(s_format_v, format);
	clientLog.out(format, s_format_v);
	va_end(s_format_v);
}

void TClient::setUpLocalPlayer() {
	localPlayer = new TPlayer(this, nullptr, 2);
	localPlayer->setType(PLTYPE_CLIENT);
	localPlayer->loadAccount("localplayer");
	localPlayer->setLoaded(true);
	localPlayer->warp(localPlayer->getLevelName(), localPlayer->getX(), localPlayer->getY(), -1);
	localPlayer->setLocalPlayer(true);
	addPlayer(localPlayer);
}

// Called when the TClient is put into its own thread.
void TClient::operator()()
{
	running = true;
	while (running)
	{
		// Do a server loop.
		doMain();

		// Clean up deleted players here.
		cleanupDeletedPlayers();

		// Check if we should do a restart.
		if (doRestart)
		{
			doRestart = false;
			cleanup();
			int ret = init(
#ifndef __AMIGA__
					overrideIP, overridePort, overrideLocalIP, overrideInterface
#endif
					);
			if (ret != 0)
				break;
		}

		if (shutdownProgram)
			running = false;
	}
	cleanup();
}

void TClient::cleanupDeletedPlayers()
{
	if (deletedPlayers.empty()) return;
	for (auto i = deletedPlayers.begin(); i != deletedPlayers.end();)
	{
		TPlayer* player = *i;
		if (player == nullptr)
		{
			i = deletedPlayers.erase(i);
			continue;
		}

#ifdef V8NPCSERVER
		IScriptWrapped<TPlayer> *playerObject = player->getScriptObject();
		if (playerObject != nullptr)
		{
			// Process last script events for this player
			if (!player->isProcessed())
			{
				// Leave the level now while the player object is still alive
				if (player->getLevel() != nullptr)
					player->leaveLevel();

				// Send event to server that player is logging out
				if (player->isLoaded() && (player->getType() & PLTYPE_ANYPLAYER))
				{
					for (auto & it : npcNameList)
					{
						TNPC *npcObject = it.second;
						npcObject->queueNpcAction("npc.playerlogout", player);
					}
				}

				// Set processed
				player->setProcessed();
			}

			// If we just added events to the player, we will have to wait for them to run before removing player.
			if (playerObject->isReferenced())
			{
				SCRIPTENV_D("Reference count: %d\n", playerObject->getReferenceCount());
				++i;
				continue;
			}
		}
#endif

		// Get rid of the player now.
		playerIds[player->getId()] = nullptr;
		for (auto j = playerList.begin(); j != playerList.end();)
		{
			TPlayer* p = *j;
			if (p == player)
			{
#ifndef __AMIGA__
				// Unregister the player.
				sockManager.unregisterSocket(p);
#endif
				delete p;
				j = playerList.erase(j);
			}
			else ++j;
		}

		i = deletedPlayers.erase(i);
	}
	//deletedPlayers.clear();
}

void TClient::cleanup()
{
	// Close our UPNP port forward.
	// First, make sure the thread has completed already.
	// This can cause an issue if the server is about to be deleted.
#ifdef UPNP
	try {
		upnp_thread.join();
		upnp.remove_all_forwarded_ports();
	} catch (std::exception &ex) {
		serverlog.out("[Exception]\t%s\n", ex.what());
	}
#endif

	// Save translations.
	this->TS_Save();

	// Save server flags.
	saveServerFlags();

#ifdef V8NPCSERVER
	// Save npcs
	saveNpcs();

	// npc-server will be cleared from playerlist, so lets invalidate the pointer here
	mNpcServer = nullptr;
#endif

	for (auto i = playerList.begin(); i != playerList.end(); )
	{
		delete *i;
		i = playerList.erase(i);
	}
	playerIds.clear();
	playerList.clear();

	for (auto i = levelList.begin(); i != levelList.end(); )
	{
		delete *i;
		i = levelList.erase(i);
	}
	levelList.clear();

	for (auto i = mapList.begin(); i != mapList.end(); )
	{
		delete *i;
		i = mapList.erase(i);
	}
	mapList.clear();

	for (auto i = npcList.begin(); i != npcList.end(); )
	{
		delete *i;
		i = npcList.erase(i);
	}
	npcIds.clear();
	npcList.clear();
	npcNameList.clear();

	saveWeapons();
	for (auto i = weaponList.begin(); i != weaponList.end(); )
	{
		delete i->second;
		weaponList.erase(i++);
	}
	weaponList.clear();

#ifdef V8NPCSERVER
	// Clean up the script engine
	mScriptEngine.Cleanup();
#endif

#ifndef __AMIGA__
	playerSock.disconnect();
	serverlist.getSocket()->disconnect();

	// Clean up the socket manager.  Pass false so we don't cause a crash.
	sockManager.cleanup(false);
#endif
}

void TClient::restart()
{
	doRestart = true;
}

constexpr std::chrono::nanoseconds timestep(std::chrono::milliseconds(50));

bool TClient::doMain()
{
#ifndef __AMIGA__
	// Update our socket manager.
	sockManager.update(0, 5000);		// 5ms
#endif

	// Current time
	auto currentTimer = std::chrono::high_resolution_clock::now();

#ifdef V8NPCSERVER
	// Run scripts every 0.05 seconds
	auto delta_time = currentTimer - lastScriptTimer;
	lastScriptTimer = currentTimer;
	accumulator += std::chrono::duration_cast<std::chrono::nanoseconds>(delta_time);

	// Accumulator for timeout / scheduled events
	bool updated = false;
	while (accumulator >= timestep) {
		accumulator -= timestep;

		mScriptEngine.RunScripts(true);
		updated = true;
	}

	// Run any queued actions anyway
	if (!updated)
		mScriptEngine.RunScripts();
#endif

	// Every second, do some events.
	auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(currentTimer - lastTimer);
	if (time_diff.count() >= 1000)
	{
		lastTimer = currentTimer;
		doTimedEvents();

	}

	if (gameWindow != nullptr)
		gameWindow->doMain();

	return true;
}

bool TClient::doTimedEvents()
{
#ifndef __AMIGA__
	// Do serverlist events.
	serverlist.doTimedEvents();
#endif
	// Do player events.
	{
		for (auto & i : playerList)
		{
			auto *player = (TPlayer*)i;
			if (player == nullptr)
				continue;

			if (player->isNPCServer())
				continue;

			if (!player->doTimedEvents())
				this->deletePlayer(player);
		}
	}

	// Do level events.
	{
		for (auto level : levelList)
		{
				if (level == nullptr)
				continue;

			level->doTimedEvents();
		}

		// Group levels.
		for (auto & groupLevel : groupLevels)
		{
			for (auto & j : groupLevel.second)
			{
				TLevel* level = j.second;
				if (level == nullptr)
					continue;

				level->doTimedEvents();
			}
		}
	}

	// Send NW time.
	auto time_diff = std::chrono::duration_cast<std::chrono::seconds>(lastTimer - lastNWTimer);
	if (time_diff.count() >= 5)
	{
		lastNWTimer = lastTimer;
		sendPacketToAll(CString() >> (char)PLO_NEWWORLDTIME << CString().writeGInt4(getNWTime()));

#ifndef __AMIGA__
		// TODO(joey): no no no no no no no this is not how it is done! a server will announce to the serverlist a new player
		// and then it will be added! you can't just spam everything!!!
		TServerList* list = this->getServerList();
		if (list)
			list->sendPacket(CString() >> (char)SVO_REQUESTLIST >> (short)0 << CString(CString() << "_" << "\n" << "GraalEngine" << "\n" << "lister" << "\n" << "simpleserverlist" << "\n").gtokenizeI());
#endif
	}

	// Stuff that happens every minute.
	time_diff = std::chrono::duration_cast<std::chrono::seconds>(lastTimer - last1mTimer);
	if (time_diff.count() >= 60)
	{
		last1mTimer = lastTimer;

		// Save server flags.
		this->saveServerFlags();
	}

	// Stuff that happens every 3 minutes.
	time_diff = std::chrono::duration_cast<std::chrono::seconds>(lastTimer - last3mTimer);
	if (time_diff.count() >= 180)
	{
		last3mTimer = lastTimer;

		// TODO(joey): probably a better way to do this..

		// Resynchronize the file systems.
		filesystem_accounts.resync();
		for (auto & i : filesystem)
			i.resync();
	}

	// Save stuff every 5 minutes.
	time_diff = std::chrono::duration_cast<std::chrono::seconds>(lastTimer - last5mTimer);
	if (time_diff.count() >= 300)
	{
		last5mTimer = lastTimer;

		// Reload some server settings.
		loadAllowedVersions();
		loadServerMessage();
		loadIPBans();

		// Save some stuff.
		// TODO(joey): Is this really needed? We save weapons to disk when it is updated or created anyway..
		saveWeapons();
#ifdef V8NPCSERVER
		saveNpcs();
#endif

		// Check all of the instanced maps to see if the players have left.
		if (!groupLevels.empty())
		{
			for (auto i = groupLevels.begin(); i != groupLevels.end();)
			{
				// Check if any players are found.
				bool playersFound = false;
				for (auto & j : (*i).second)
				{
					TLevel* level = j.second;
					if (!level->getPlayerList()->empty())
					{
						playersFound = true;
						break;
					}
				}

				// If no players are found, delete all the levels in this instance.
				if (!playersFound)
				{
					for (auto & j : (*i).second)
					{
						TLevel* level = j.second;
						delete level;
					}
					(*i).second.clear();
					groupLevels.erase(i++);
				}
				else ++i;
			}
		}
	}

	return true;
}

#ifndef __AMIGA__
bool TClient::onRecv()
{
	// Create socket.
	CSocket *newSock = playerSock.accept();
	if (newSock == nullptr)
		return true;

	// Create the new player.
	auto *newPlayer = new TPlayer(this, newSock, 0);

	// Add the player to the server
	if (!addPlayer(newPlayer))
	{
		delete newPlayer;
		return false;
	}

	// Add them to the socket manager.
	sockManager.registerSocket((CSocketStub*)newPlayer);

	return true;
}
#endif
/////////////////////////////////////////////////////

void TClient::loadAllFolders()
{
	for (auto & i : filesystem)
		i.clear();

	filesystem[0].addDir("world");
	if (settings.getStr("sharefolder").length() > 0)
	{
		std::vector<CString> folders = settings.getStr("sharefolder").tokenize(",");
		for (auto & folder : folders)
			filesystem[0].addDir(folder.trim());
	}
}

void TClient::loadFolderConfig()
{
	for (auto & i : filesystem)
		i.clear();

	foldersConfig = CString::loadToken(CString() << runnerPath << "config/foldersconfig.txt", "\n", true);
	for (auto & i : foldersConfig)
	{
		// No comments.
		int cLoc = -1;
		if ((cLoc = i.find("#")) != -1)
			i.removeI(cLoc);
		i.trimI();
		if (i.length() == 0) continue;

		// Parse the line.
		CString type = i.readString(" ");
		CString config = i.readString("");
		type.trimI();
		config.trimI();
		CFileSystem::fixPathSeparators(&config);

		// Get the directory.
		CString dirNoWild;
		int pos = -1;
		if ((pos = config.findl(CFileSystem::getPathSeparator())) != -1)
			dirNoWild = config.remove(pos + 1);
		CString dir = CString("world/") << dirNoWild;
		CString wildcard = config.remove(0, dirNoWild.length());

		// Find out which file system to add it to.
		CFileSystem* fs = getFileSystemByType(type);

		// Add it to the appropriate file system.
		if (fs != nullptr)
		{
			fs->addDir(dir, wildcard);
			clientLog.out("\tadding %s [%s] to %s\n", dir.text(), wildcard.text(), type.text());
		}
		filesystem[0].addDir(dir, wildcard);
	}
}

int TClient::loadConfigFiles()
{
	// TODO(joey): /reloadconfig reloads this, but things like server flags, weapons and npcs probably shouldn't be reloaded.
	//	Move them out of here?
	clientLog.out(":: Loading configuration...\n");

	// Load Settings
	clientLog.out("Loading settings...\n");
	loadSettings();
#ifndef __AMIGA__
	// Load Admin Settings
	clientLog.out("[%s]      Loading admin settings...\n");
	loadAdminSettings();

	// Load allowed versions.
	clientLog.out("[%s]      Loading allowed client versions...\n");
	loadAllowedVersions();
#endif

	// Load folders config and file system.
	clientLog.out("Folder config: ");
	if ( !settings.getBool("nofoldersconfig", false))
	{
		clientLog.append("Enabled\n");
	} else clientLog.append("Disabled\n");
	clientLog.out("Loading file system...\n");
	loadFileSystem();

#ifndef __AMIGA__
	// Load server flags.
	clientLog.out("\t\tLoading serverflags.txt...\n");
	loadServerFlags();

	// Load server message.
	clientLog.out("\t\tLoading config/servermessage.html...\n");
	loadServerMessage();

	// Load IP bans.
	clientLog.out("\t\tLoading config/ipbans.txt...\n");
	loadIPBans();
#endif

	// Load weapons.
	clientLog.out("Loading weapons...\n");
	loadWeapons(true);

	// Load classes.
	clientLog.out("Loading classes...\n");
	loadClasses(true);

	// Load maps.
	clientLog.out("Loading maps...\n");
	loadMaps(true);

#ifdef V8NPCSERVER
	// Load database npcs.
	clientLog.out("Loading npcs...\n");
	loadNpcs(true);
#endif

	// Load translations.
	clientLog.out("Loading translations...\n");
	loadTranslations();

	// Load word filter.
	clientLog.out("Loading word filter...\n");
	loadWordFilter();

	return 0;
}

void TClient::loadSettings()
{
	settings.setSeparator("=");
	settings.loadFile(CString() << runnerPath << "config/serveroptions.txt");
	if (!settings.isOpened())
		clientLog.out("\t** [Error] Could not open config/serveroptions.txt.  Will use default config.\n");

	// Load status list.
	statusList = settings.getStr("playerlisticons", "Online,Away,DND,Eating,Hiding,No PMs,RPing,Sparring,PKing").tokenize(",");
#ifndef __AMIGA__
	// Send our ServerHQ info in case we got changed the staffonly setting.
	getServerList()->sendServerHQ();
#endif
}

void TClient::loadAdminSettings()
{
	adminsettings.setSeparator("=");
	adminsettings.loadFile(CString() << runnerPath << "config/adminconfig.txt");
	if (!adminsettings.isOpened())
		clientLog.out("\t** [Error] Could not open config/adminconfig.txt.  Will use default config.\n");
#ifndef __AMIGA__
	else getServerList()->sendServerHQ();
#endif
}

void TClient::loadAllowedVersions()
{
	CString versions;
	versions.load(CString() << runnerPath << "config/allowedversions.txt");
	versions = removeComments(versions);
	versions.removeAllI("\r");
	versions.removeAllI("\t");
	versions.removeAllI(" ");
	allowedVersions = versions.tokenize("\n");
	allowedVersionString.clear();
	for (auto & allowedVersion : allowedVersions)
	{
		if (!allowedVersionString.isEmpty())
			allowedVersionString << ", ";

		int loc = allowedVersion.find(":");
		if (loc == -1)
			allowedVersionString << getVersionString(allowedVersion, PLTYPE_ANYCLIENT);
		else
		{
			CString s = allowedVersion.subString(0, loc);
			CString f = allowedVersion.subString(loc + 1);
			int vid = getVersionID(s);
			int vid2 = getVersionID(f);
			if (vid != -1 && vid2 != -1)
				allowedVersionString << getVersionString(s, PLTYPE_ANYCLIENT) << " - " << getVersionString(f, PLTYPE_ANYCLIENT);
		}
	}
}

void TClient::loadFileSystem()
{
	for (auto & i : filesystem)
		i.clear();
	filesystem_accounts.clear();
	filesystem_accounts.addDir("accounts");
	if (settings.getBool("nofoldersconfig", false))
		loadAllFolders();
	else
		loadFolderConfig();
}

void TClient::loadServerFlags()
{
	std::vector<CString> lines = CString::loadToken(CString() << runnerPath << "serverflags.txt", "\n", true);
	for (auto & line : lines)
		this->setFlag(line, false);
}

void TClient::loadServerMessage()
{
	servermessage.load(CString() << runnerPath << "config/servermessage.html");
	servermessage.removeAllI("\r");
	servermessage.replaceAllI("\n", " ");
}

void TClient::loadIPBans()
{
	ipBans = CString::loadToken(CString() << runnerPath << "config/ipbans.txt", "\n", true);
}

void TClient::loadClasses(bool print)
{
	CFileSystem scriptFS(this);
	scriptFS.addDir("scripts", "*.txt");
	std::map<CString, CString> *scriptFileList = scriptFS.getFileList();
	for (auto & it : *scriptFileList)
	{
		std::string className = it.first.subString(0, it.first.length() - 4).text();

		CString scriptData;
		scriptData.load(it.second);
		classList[className] = scriptData.text();
	}
}

void TClient::loadWeapons(bool print)
{
	CFileSystem weaponFS(this);
	weaponFS.addDir("weapons", "weapon*.txt");
	std::map<CString, CString> *weaponFileList = weaponFS.getFileList();
	for (auto & i : *weaponFileList)
	{
		TWeapon *weapon = TWeapon::loadWeapon(i.first, this);
		if (weapon == 0) continue;
		weapon->setModTime(weaponFS.getModTime(i.first));

		// Check if the weapon exists.
		if (weaponList.find(weapon->getName()) == weaponList.end())
		{
			weaponList[weapon->getName()] = weapon;
			if (print) clientLog.out("\t%s\n", weapon->getName().text());
		}
		else
		{
			// If the weapon exists, and the version on disk is newer, reload it.
			TWeapon* w = weaponList[weapon->getName()];
			if (w->getModTime() < weapon->getModTime())
			{
				delete w;
				weaponList[weapon->getName()] = weapon;
				updateWeaponForPlayers(weapon);
				if (print) clientLog.out("\t%s [updated]\n", weapon->getName().text());
			}
			else
			{
				// TODO(joey): even though were deleting the weapon because its skipped, its still queuing its script action
				//	and attempting to execute it. Technically the code needs to be run again though, will fix soon.
				if (print) clientLog.out("\t%s [skipped]\n", weapon->getName().text());
				delete weapon;
			}
		}
	}

	// Add the default weapons.
	if (weaponList.find("bow") == weaponList.end())	weaponList["bow"] = new TWeapon(this, TLevelItem::getItemId("bow"));
	if (weaponList.find("bomb") == weaponList.end()) weaponList["bomb"] = new TWeapon(this, TLevelItem::getItemId("bomb"));
	if (weaponList.find("superbomb") == weaponList.end()) weaponList["superbomb"] = new TWeapon(this, TLevelItem::getItemId("superbomb"));
	if (weaponList.find("fireball") == weaponList.end()) weaponList["fireball"] = new TWeapon(this, TLevelItem::getItemId("fireball"));
	if (weaponList.find("fireblast") == weaponList.end()) weaponList["fireblast"] = new TWeapon(this, TLevelItem::getItemId("fireblast"));
	if (weaponList.find("nukeshot") == weaponList.end()) weaponList["nukeshot"] = new TWeapon(this, TLevelItem::getItemId("nukeshot"));
	if (weaponList.find("joltbomb") == weaponList.end()) weaponList["joltbomb"] = new TWeapon(this, TLevelItem::getItemId("joltbomb"));
}

void TClient::loadMaps(bool print)
{
	// Remove existing maps.
	for (auto i = mapList.begin(); i != mapList.end(); )
	{
		TMap* map = *i;
		for (auto & j : playerList)
		{
			if (j->getMap() == map)
				j->setMap(0);
		}
		delete map;
		i = mapList.erase(i);
	}

	// Load gmaps.
	std::vector<CString> gmaps = settings.getStr("gmaps").guntokenize().tokenize("\n");
	for (auto & i : gmaps)
	{
		// Check for blank lines.
		if (i == "\r") continue;

		// Load the gmap.
		TMap* gmap = new TMap(MAPTYPE_GMAP);
		if ( !gmap->load(CString() << (CString)i << ".gmap", this))
		{
			if (print) clientLog.out(CString() << "[" << name << "] " << "** [Error] Could not load " << i << ".gmap" << "\n");
			delete gmap;
			continue;
		}

		if (print) clientLog.out("\t[gmap] %s\n", i.text());
		mapList.push_back(gmap);
	}

	// Load bigmaps.
	std::vector<CString> bigmaps = settings.getStr("maps").guntokenize().tokenize("\n");
	for (auto & i : bigmaps)
	{
		// Check for blank lines.
		if (i == "\r") continue;

		// Load the bigmap.
		TMap* bigmap = new TMap(MAPTYPE_BIGMAP);
		if ( !bigmap->load((i).trim(), this))
		{
			if (print) clientLog.out(CString() << "[" << name << "] " << "** [Error] Could not load " << i << "\n");
			delete bigmap;
			continue;
		}

		if (print) clientLog.out("\t[bigmap] %s\n", i.text());
		mapList.push_back(bigmap);
	}

	// Load group maps.
	std::vector<CString> groupmaps = settings.getStr("groupmaps").guntokenize().tokenize("\n");
	for (auto & groupmap : groupmaps)
	{
		// Check for blank lines.
		if (groupmap == "\r") continue;

		// Determine the type of map we are loading.
		CString ext(getExtension(groupmap));
		ext.toLowerI();

		// Create the new map based on the file extension.
		TMap* gmap = 0;
		if (ext == ".txt")
			gmap = new TMap(MAPTYPE_BIGMAP, true);
		else if (ext == ".gmap")
			gmap = new TMap(MAPTYPE_GMAP, true);
		else continue;

		// Load the map.
		if ( !gmap->load(CString() << groupmap, this))
		{
			if (print) clientLog.out(CString() << "[" << name << "] " << "** [Error] Could not load " << groupmap << "\n");
			delete gmap;
			continue;
		}

		if (print) clientLog.out("\t[group map] %s\n", groupmap.text());
		mapList.push_back(gmap);
	}
}

#ifdef V8NPCSERVER
void TClient::loadNpcs(bool print)
{
	CFileSystem npcFS(this);
	npcFS.addDir("npcs", "npc*.txt");
	std::map<CString, CString> *npcFileList = npcFS.getFileList();
	for (auto & it : *npcFileList)
	{
		bool loaded = false;

		// Create the npc
		TNPC *newNPC = new TNPC("", "", 30, 30.5, this, nullptr, false);
		if (newNPC->loadNPC(it.second))
		{
			int npcId = newNPC->getId();
			if (npcId < 1000)
			{
				printf("Database npcs must be greater than 1000\n");
			}
			else if (npcId < npcIds.size() && npcIds[npcId] != 0)
			{
				printf("Error creating database npc: Id is in use!\n");
			}
			else
			{
				// Assign id to npc
				if (npcIds.size() <= npcId)
					npcIds.resize((size_t)npcId + 10);

				npcIds[npcId] = newNPC;
				npcList.push_back(newNPC);
				assignNPCName(newNPC, newNPC->getName());

				// Add npc to level
				TLevel *level = newNPC->getLevel();
				if (level)
					level->addNPC(newNPC);

				loaded = true;
			}
		}

		if (!loaded)
			delete newNPC;
	}
}
#endif

void TClient::loadTranslations()
{
	this->TS_Reload();
}

void TClient::loadWordFilter()
{
	wordFilter.load(CString() << runnerPath << "config/rules.txt");
}

void TClient::saveServerFlags()
{
	CString out;
	for (auto & mServerFlag : mServerFlags)
		out << mServerFlag.first << "=" << mServerFlag.second << "\r\n";
	out.save(CString() << runnerPath << "serverflags.txt");
}

void TClient::saveWeapons()
{
	CFileSystem weaponFS(this);
	weaponFS.addDir("weapons", "weapon*.txt");
	std::map<CString, CString> *weaponFileList = weaponFS.getFileList();

	for (auto & it : weaponList)
	{
		TWeapon *weaponObject = it.second;
		if (weaponObject->isDefault())
			continue;

		// TODO(joey): add a function to weapon to get the filename?
		CString weaponFile = CString("weapon") << it.first << ".txt";
		time_t mod = weaponFS.getModTime(weaponFile);
		if (weaponObject->getModTime() > mod)
		{
			// The weapon in memory is newer than the weapon on disk.  Save it.
			weaponObject->saveWeapon();
			weaponFS.setModTime(weaponFS.find(weaponFile), weaponObject->getModTime());
		}
	}
}

#ifdef V8NPCSERVER

void TClient::saveNpcs()
{
	for (auto npc : npcList)
	{
			if (npc->getPersist())
			npc->saveNPC();
	}
}

std::vector<std::pair<double, std::string>> TClient::calculateNpcStats()
{
	std::vector<std::pair<double, std::string>> script_profiles;

	// Iterate npcs
	for (auto npc : npcList)
	{
		ScriptExecutionContext *context = npc->getExecutionContext();
		std::pair<unsigned int, double> executionData = context->getExecutionData();
		if (executionData.second > 0.0)
		{
			std::string npcName = npc->getName();
			if (npcName.empty())
				npcName = "Level npc " + std::to_string(npc->getId());

			TLevel *npcLevel = npc->getLevel();
			if (npcLevel != nullptr) {
				npcName.append(" (in level ").append(npcLevel->getLevelName().text()).
					append(" at pos (").append(CString((float)npc->getPixelX() / 16.0f).text()).
					append(", ").append(CString((float)npc->getPixelY() / 16.0f).text()).append(")");
			}

			script_profiles.emplace_back(executionData.second, npcName);
		}
	}

	// Iterate weapons
	for (auto & it : weaponList)
	{
		TWeapon *weapon = it.second;
		ScriptExecutionContext *context = weapon->getExecutionContext();
		std::pair<unsigned int, double> executionData = context->getExecutionData();

		if (executionData.second > 0.0)
		{
			std::string weaponName("Weapon ");
			weaponName.append(it.first.text());
			script_profiles.emplace_back(executionData.second, weaponName);
		}
	}

	std::sort(script_profiles.rbegin(), script_profiles.rend());
	return script_profiles;
}

void TClient::reportScriptException(const ScriptRunError& error)
{
	std::string error_message = error.getErrorString();
	sendToNC(error_message);
	getScriptLog().out(error_message + "\n");
}

void TClient::reportScriptException(const std::string& error_message)
{
	sendToNC(error_message);
	getScriptLog().out(error_message + "\n");
}

#endif

/////////////////////////////////////////////////////

TPlayer* TClient::getPlayer(const unsigned short id, int type) const
{
	if (id >= (unsigned short)playerIds.size())
		return nullptr;

	if (playerIds[id] == nullptr)
		return nullptr;

	// Check if its the type of player we are looking for
	if (!(playerIds[id]->getType() & type))
		return nullptr;

	return playerIds[id];
}

TPlayer* TClient::getPlayer(const CString& account, int type) const
{
	for (auto i : playerList)
	{
		auto *player = (TPlayer*)i;
		if (player == nullptr)
			continue;

		// Check if its the type of player we are looking for
		if (!(player->getType() & type))
			continue;

		// Compare account names.
		if (player->getAccountName().toLower() == account.toLower())
			return player;
	}

	return nullptr;
}

/*
TPlayer* TClient::getPlayer(const unsigned short id, bool includeRC) const
{
	if (id >= (unsigned short)playerIds.size()) return 0;
	if (playerIds[id] == nullptr) return 0;
	if (!includeRC && playerIds[id]->isControlClient()) return 0;
	if (playerIds[id]->isHiddenClient()) return 0;
	return playerIds[id];
}

TPlayer* TClient::getPlayer(const CString& account, bool includeRC) const
{
	for (std::vector<TPlayer *>::const_iterator i = playerList.begin(); i != playerList.end(); ++i)
	{
		TPlayer *player = (TPlayer*)*i;
		if (player == 0 || player->isHiddenClient())
			continue;

		if (!includeRC && player->isRC())
			continue;

		// Compare account names.
		if (player->getAccountName().toLower() == account.toLower())
			return player;
	}
	return 0;
}

TPlayer* TClient::getRC(const unsigned short id) const
{
	if (id >= (unsigned short)playerIds.size()) return 0;
	if (playerIds[id] == nullptr) return 0;
	if (!playerIds[id]->isRC()) return 0;
	return playerIds[id];
}

TPlayer* TClient::getRC(const CString& account) const
{
	for (std::vector<TPlayer *>::const_iterator i = playerList.begin(); i != playerList.end(); ++i)
	{
		TPlayer *player = (TPlayer*)*i;
		if (player == 0 || !player->isRC())
			continue;

		// Compare account names.
		if (player->getAccountName().toLower() == account.toLower())
			return player;
	}
	return 0;
}
*/

TLevel* TClient::getLevel(const CString& pLevel)
{
	return TLevel::findLevel(pLevel, this);
}

TMap* TClient::getMap(const CString& name) const
{
	for (auto map : mapList)
	{
			if (map->getMapName() == name)
			return map;
	}
	return nullptr;
}

TMap* TClient::getMap(const TLevel* pLevel) const
{
	if (pLevel == nullptr) return nullptr;

	for (auto pMap : mapList)
	{
			if (pMap->isLevelOnMap(pLevel->getLevelName()))
			return pMap;
	}
	return nullptr;
}

TWeapon* TClient::getWeapon(const CString& name)
{
	return (weaponList.find(name) != weaponList.end() ? weaponList[name] : nullptr);
}

CString TClient::getFlag(const std::string& pFlagName)
{
#ifdef V8NPCSERVER
	if (mServerFlags.find(pFlagName) != mServerFlags.end())
		return mServerFlags[pFlagName];
	return "";
#else
	return mServerFlags[pFlagName];
#endif
}

CFileSystem* TClient::getFileSystemByType(CString& type)
{
	// Find out the filesystem.
	int fs = -1;
	int j = 0;
	while (filesystemTypes[j] != nullptr)
	{
		if (type.comparei(CString(filesystemTypes[j])))
		{
			fs = j;
			break;
		}
		++j;
	}

	if (fs == -1) return nullptr;
	return &filesystem[fs];
}

#ifdef V8NPCSERVER
void TClient::assignNPCName(TNPC *npc, const std::string& name)
{
	std::string newName = name;
	int num = 0;
	while (npcNameList.find(newName) != npcNameList.end())
		newName = name + std::to_string(++num);

	npc->setName(newName);
	npcNameList[newName] = npc;
}

void TClient::removeNPCName(TNPC *npc)
{
	auto npcIter = npcNameList.find(npc->getName());
	if (npcIter != npcNameList.end())
		npcNameList.erase(npcIter);
}

TNPC* TClient::addServerNpc(int npcId, float pX, float pY, TLevel *pLevel, bool sendToPlayers)
{
	// Force database npc ids to be >= 1000
	if (npcId < 1000)
	{
		printf("Database npcs need to be greater than 1000\n");
		return nullptr;
	}

	// Make sure the npc id isn't in use
	if (npcId < npcIds.size() && npcIds[npcId] != 0)
	{
		printf("Error creating database npc: Id is in use!\n");
		return nullptr;
	}

	// Create the npc
	TNPC* newNPC = new TNPC("", "", pX, pY, this, pLevel, false);
	newNPC->setId(npcId);
	npcList.push_back(newNPC);

	if (npcIds.size() <= npcId)
		npcIds.resize((size_t)npcId + 10);
	npcIds[npcId] = newNPC;

	// Add the npc to the level
	if (pLevel)
		pLevel->addNPC(newNPC);

	// Send the NPC's props to everybody in range.
	if (sendToPlayers)
	{
		CString packet = CString() >> (char)PLO_NPCPROPS >> (int)newNPC->getId() << newNPC->getProps(0);

		// Send to level.
		TMap* map = getMap(pLevel);
		sendPacketToLevel(packet, map, pLevel, 0, true);
	}

	return newNPC;
}

void TClient::handlePM(TPlayer * player, const CString & message)
{
	if (!mPmHandlerNpc)
	{
		CString npcServerMsg;
		npcServerMsg = "I am the npcserver for\nthis game server. Almost\nall npc actions are controlled\nby me.";
		player->sendPacket(CString() >> (char)PLO_PRIVATEMESSAGE >> (short)mNpcServer->getId() << "\"\"," << npcServerMsg.gtokenize());
		return;
	}

	// TODO(joey): This sets the first argument as the npc object, so we can't use it here for now.
	//mPmHandlerNpc->queueNpcEvent("npcserver.playerpm", true, player->getScriptObject(), std::string(message.text()));

	printf("Msg: %s\n", std::string(message.text()).c_str());

	ScriptAction *scriptAction = mScriptEngine.CreateAction("npcserver.playerpm", player->getScriptObject(), std::string(message.text()));
	mPmHandlerNpc->getExecutionContext()->addAction(scriptAction);
	mScriptEngine.RegisterNpcUpdate(mPmHandlerNpc);
}

void TClient::setPMFunction(TNPC *npc, IScriptFunction *function)
{
	if (npc == nullptr || function == nullptr)
	{
		mPmHandlerNpc = nullptr;
		mScriptEngine.removeCallBack("npcserver.playerpm");
		return;
	}

	mScriptEngine.setCallBack("npcserver.playerpm", function);
	mPmHandlerNpc = npc;
}

#endif

TNPC* TClient::addNPC(const CString& pImage, const CString& pScript, float pX, float pY, TLevel* pLevel, bool pLevelNPC, bool sendToPlayers)
{
	// New Npc
	TNPC* newNPC = new TNPC(pImage, pScript, pX, pY, this, pLevel, pLevelNPC);
	npcList.push_back(newNPC);

	// Assign NPC Id
	bool assignedId = false;
	for (unsigned int i = 10000; i < npcIds.size(); ++i)
	{
		if (npcIds[i] == nullptr)
		{
			npcIds[i] = newNPC;
			newNPC->setId(i);
			assignedId = true;
			break;
		}
	}

	// Assign NPC Id
	if ( !assignedId )
	{
		newNPC->setId(npcIds.size());
		npcIds.push_back(newNPC);
	}

	// Send the NPC's props to everybody in range.
	if (sendToPlayers)
	{
		CString packet = CString() >> (char)PLO_NPCPROPS >> (int)newNPC->getId() << newNPC->getProps(0);

		// Send to level.
		TMap* map = getMap(pLevel);
		sendPacketToLevel(packet, map, pLevel, 0, true);
	}

	return newNPC;
}

bool TClient::deleteNPC(const unsigned int pId, bool eraseFromLevel)
{
	// Grab the NPC.
	TNPC* npc = getNPC(pId);
	return npc == nullptr ? false : deleteNPC(npc, eraseFromLevel);

}

bool TClient::deleteNPC(TNPC* npc, bool eraseFromLevel)
{
	if (npc == nullptr) return false;
	if (npc->getId() >= npcIds.size()) return false;

	// Remove the NPC from all the lists.
	npcIds[npc->getId()] = nullptr;

	for (auto i = npcList.begin(); i != npcList.end(); )
	{
		if ((*i) == npc)
			i = npcList.erase(i);
		else ++i;
	}

	TLevel *level = npc->getLevel();

	if (level)
	{
		// Remove the NPC from the level
		if (eraseFromLevel)
			level->removeNPC(npc);

		// Tell the clients to delete the NPC.
		bool isOnMap = level->getMap() != nullptr;
		CString tmpLvlName = (isOnMap ? level->getMap()->getMapName() : level->getLevelName());

		for (auto p : playerList)
		{
				if (p->isControlClient()) continue;

			if (isOnMap || p->getVersion() < CLVER_2_1)
				p->sendPacket(CString() >> (char)PLO_NPCDEL >> (int)npc->getId());
			else
				p->sendPacket(CString() >> (char)PLO_NPCDEL2 >> (char)tmpLvlName.length() << tmpLvlName >> (int)npc->getId());
		}
	}

#ifdef V8NPCSERVER
	// TODO(joey): Need to deal with illegal characters
	// If we persist this npc, delete the file  [ maybe should add a parameter if we should remove the npc from disk ]
	if (npc->getPersist())
	{
		CString filePath = getRunnerPath() << "npcs/npc" << npc->getName() << ".txt";
		CFileSystem::fixPathSeparators(&filePath);
		remove(filePath.text());
	}

	if (!npc->isLevelNPC())
	{
		// Remove npc name assignment
		if (!npc->getName().empty())
			removeNPCName(npc);

		// If this is the npc that handles pms, clear it
		if (mPmHandlerNpc == npc)
			mPmHandlerNpc = nullptr;
	}
#endif

	// Delete the NPC from memory.
	delete npc;

	return true;
}

bool TClient::deleteClass(const std::string& className)
{
	auto classIter = classList.find(className);
	if (classIter == classList.end())
		return false;

	classList.erase(classIter);
	CString filePath = getRunnerPath() << "scripts/" << className << ".txt";
	CFileSystem::fixPathSeparators(&filePath);
	remove(filePath.text());

	return true;
}

void TClient::updateClass(const std::string& className, const std::string& classCode)
{
	// TODO(joey): filenames...
	classList[className] = classCode;

	CString filePath = getRunnerPath() << "scripts/" << className << ".txt";
	CFileSystem::fixPathSeparators(&filePath);

	CString fileData(classCode);
	fileData.save(filePath);
}

unsigned int TClient::getFreePlayerId()
{
	unsigned int newId = 0;
	for (unsigned int i = 2; i < playerIds.size(); ++i)
	{
		if (playerIds[i] == nullptr)
		{
			newId = i;
			i = playerIds.size();
		}
	}
	if (newId == 0)
	{
		newId = playerIds.size();
		playerIds.push_back(nullptr);
	}

	return newId;
}

bool TClient::addPlayer(TPlayer *player, unsigned int id)
{
	// No id was passed, so we will fetch one
	if (id == UINT_MAX)
		id = getFreePlayerId();
	else if (playerIds.size() <= id)
		playerIds.resize((size_t)id + 10);
	else if (playerIds[id] != nullptr)
		return false;

	// Add them to the player list.
	player->setId(id);
	playerIds[id] = player;
	playerList.push_back(player);

#ifdef V8NPCSERVER
	// Create script object for player
	mScriptEngine.WrapObject(player);
#endif

	return true;
}

bool TClient::deletePlayer(TPlayer* player)
{
	if (player == nullptr)
		return true;

	// Add the player to the set of players to delete.
	if ( deletedPlayers.insert(player).second )
	{
#ifndef __AMIGA__
		// Remove the player from the serverlist.
		getServerList()->remPlayer(player->getAccountName(), player->getType());
#endif
	}

	return true;
}

void TClient::playerLoggedIn(TPlayer *player)
{
#ifndef __AMIGA__
	// Tell the serverlist that the player connected.
	getServerList()->addPlayer(player);

#ifdef V8NPCSERVER
	// Send event to server that player is logging out
	for (auto & it : npcNameList)
	{
		TNPC *npcObject = it.second;
		npcObject->queueNpcAction("npc.playerlogin", player);
	}
#endif
#endif
}

unsigned int TClient::getNWTime() const
{
	// timevar apparently subtracts 11078 days from time(0) then divides by 5.
	return ((unsigned int)time(nullptr) - 11078 * 24 * 60 * 60) / 5;
}

bool TClient::isIpBanned(const CString& ip)
{
	for (auto & ipBan : ipBans)
	{
		if (ip.match(ipBan)) return true;
	}
	return false;
}

void TClient::logToFile(const std::string & fileName, const std::string & message)
{
	CString fileNamePath = CString() << getRunnerPath().remove(0, getHomePath().length()) << "logs/";

	// Remove leading characters that may try to go up a directory
	int idx = 0;
	while (fileName[idx] == '.' || fileName[idx] == '/' || fileName[idx] == '\\')
		idx++;
	fileNamePath << fileName.substr(idx);

	CLog logFile(fileNamePath, true);
	logFile.open();
	logFile.out("\n%s\n", message.c_str());
}

/*
	TClient: Server Flag Management
*/
bool TClient::deleteFlag(const std::string& pFlagName, bool pSendToPlayers)
{
	if ( settings.getBool("dontaddserverflags", false))
		return false;

	std::unordered_map<std::string, CString>::iterator i;
	if ((i = mServerFlags.find(pFlagName)) != mServerFlags.end())
	{
		mServerFlags.erase(i);
		if (pSendToPlayers)
			sendPacketToAll(CString() >> (char)PLO_FLAGDEL << pFlagName);
		return true;
	}

	return false;
}

bool TClient::setFlag(CString pFlag, bool pSendToPlayers)
{
	std::string flagName = pFlag.readString("=").text();
	CString flagValue = pFlag.readString("");
	return this->setFlag(flagName, (flagValue.isEmpty() ? "1" : flagValue), pSendToPlayers);
}

bool TClient::setFlag(const std::string& pFlagName, const CString& pFlagValue, bool pSendToPlayers)
{
	if ( settings.getBool("dontaddserverflags", false))
		return false;

	// delete flag
	if (pFlagValue.isEmpty())
		return deleteFlag(pFlagName);

	// optimize
	if (mServerFlags[pFlagName] == pFlagValue)
		return true;

	// set flag
	if (settings.getBool("cropflags", true))
	{
		int fixedLength = 223 - 1 - pFlagName.length();
		mServerFlags[pFlagName] = pFlagValue.subString(0, fixedLength);
	}
	else mServerFlags[pFlagName] = pFlagValue;

	if (pSendToPlayers)
		sendPacketToAll(CString() >> (char)PLO_FLAGSET << pFlagName << "=" << pFlagValue);
	return true;
}

/*
	Packet-Sending Functions
*/
void TClient::sendPacketToAll(CString pPacket, TPlayer *pPlayer, bool pNpcServer) const
{
	for (auto i : playerList)
	{
		if (i == pPlayer) continue;

		i->sendPacket(pPacket);
	}
}

void TClient::sendPacketToLevel(CString pPacket, TMap* pMap, TLevel* pLevel, TPlayer* pPlayer, bool onlyGmap) const
{
	if (pMap == nullptr || (onlyGmap && pMap->getType() == MAPTYPE_BIGMAP))// || pLevel->isGroupLevel())
	{
		for (auto i : playerList)
		{
			if (i == pPlayer || !i->isClient()) continue;
			if (i->getLevel() == pLevel)
				i->sendPacket(pPacket);
		}
		return;
	}

	if (pLevel == 0) return;
	bool _groupMap = (pPlayer == 0 ? false : pPlayer->getMap()->isGroupMap());
	for (auto other : playerList)
	{
			if (!other->isClient() || other == pPlayer || other->getLevel() == 0) continue;
		if (_groupMap && pPlayer != 0 && pPlayer->getGroup() != other->getGroup()) continue;

		if (other->getMap() == pMap)
		{
			int sgmap[2] = {pMap->getLevelX(pLevel->getActualLevelName()), pMap->getLevelY(pLevel->getActualLevelName())};
			int ogmap[2];
			switch (pMap->getType())
			{
				case MAPTYPE_GMAP:
					ogmap[0] = other->getProp(PLPROP_GMAPLEVELX).readGUChar();
					ogmap[1] = other->getProp(PLPROP_GMAPLEVELY).readGUChar();
					break;

				default:
				case MAPTYPE_BIGMAP:
					ogmap[0] = pMap->getLevelX(other->getLevel()->getActualLevelName());
					ogmap[1] = pMap->getLevelY(other->getLevel()->getActualLevelName());
					break;
			}

			if (abs(ogmap[0] - sgmap[0]) < 2 && abs(ogmap[1] - sgmap[1]) < 2)
				other->sendPacket(pPacket);
		}
	}
}

void TClient::sendPacketToLevel(CString pPacket, TMap* pMap, TPlayer* pPlayer, bool sendToSelf, bool onlyGmap) const
{
	if (pPlayer->getLevel() == 0) return;

	if (pMap == 0 || (onlyGmap && pMap->getType() == MAPTYPE_BIGMAP) || pPlayer->getLevel()->isSingleplayer() == true)
	{
		TLevel* level = pPlayer->getLevel();
		if (level == 0) return;
		for (auto i : playerList)
		{
			if ((i == pPlayer && !sendToSelf) || !i->isClient()) continue;
			if (i->getLevel() == level)
				i->sendPacket(pPacket);
		}
		return;
	}

	bool _groupMap = pPlayer->getMap()->isGroupMap();
	for (auto i : playerList)
	{
		if (!i->isClient()) continue;
		if (i == pPlayer)
		{
			if (sendToSelf) pPlayer->sendPacket(pPacket);
			continue;
		}
		if (i->getLevel() == 0) continue;
		if (_groupMap && pPlayer->getGroup() != i->getGroup()) continue;

		if (i->getMap() == pMap)
		{
			int ogmap[2], sgmap[2];
			switch (pMap->getType())
			{
				case MAPTYPE_GMAP:
					ogmap[0] = i->getProp(PLPROP_GMAPLEVELX).readGUChar();
					ogmap[1] = i->getProp(PLPROP_GMAPLEVELY).readGUChar();
					sgmap[0] = pPlayer->getProp(PLPROP_GMAPLEVELX).readGUChar();
					sgmap[1] = pPlayer->getProp(PLPROP_GMAPLEVELY).readGUChar();
					break;

				default:
				case MAPTYPE_BIGMAP:
					ogmap[0] = pMap->getLevelX(i->getLevel()->getActualLevelName());
					ogmap[1] = pMap->getLevelY(i->getLevel()->getActualLevelName());
					sgmap[0] = pMap->getLevelX(pPlayer->getLevel()->getActualLevelName());
					sgmap[1] = pMap->getLevelY(pPlayer->getLevel()->getActualLevelName());
					break;
			}

			if (abs(ogmap[0] - sgmap[0]) < 2 && abs(ogmap[1] - sgmap[1]) < 2)
				i->sendPacket(pPacket);
		}
	}
}

void TClient::sendPacketTo(int who, CString pPacket, TPlayer* pPlayer) const
{
	for (auto i : playerList)
	{
		if (i == pPlayer) continue;
		if (i->getType() & who)
			i->sendPacket(pPacket);
	}
}

/*
	NPC-Server Functionality
*/
bool TClient::NC_AddWeapon(TWeapon *pWeaponObj)
{
	if (pWeaponObj == nullptr)
		return false;

	weaponList[pWeaponObj->getName()] = pWeaponObj;
	return true;
}

bool TClient::NC_DelWeapon(const CString& pWeaponName)
{
	// Definitions
	TWeapon *weaponObj = getWeapon(pWeaponName);
	if (weaponObj == nullptr || weaponObj->isDefault())
		return false;

	// Delete from File Browser
	CString name = pWeaponName;
	name.replaceAllI("\\", "_");
	name.replaceAllI("/", "_");
	name.replaceAllI("*", "@");
	name.replaceAllI(":", ";");
	name.replaceAllI("?", "!");
	CString filePath = getRunnerPath() << "weapons/weapon" << name << ".txt";
	CFileSystem::fixPathSeparators(&filePath);
	remove(filePath.text());

	// Delete from Memory
	mapRemove<CString, TWeapon *>(weaponList, weaponObj);
	delete weaponObj;

	// Delete from Players
	sendPacketTo(PLTYPE_ANYCLIENT, CString() >> (char)PLO_NPCWEAPONDEL << pWeaponName);
	return true;
}

void TClient::updateWeaponForPlayers(TWeapon *pWeapon)
{
	// Update Weapons
	for (auto player : playerList)
	{
			if (!player->isClient())
			continue;

		if (player->hasWeapon(pWeapon->getName()))
		{
			player->sendPacket(CString() >> (char)PLO_NPCWEAPONDEL << pWeapon->getName());
			player->sendPacket(CString() << pWeapon->getWeaponPacket());
		}
	}
}

/*
	Translation Functionality
*/
bool TClient::TS_Load(const CString& pLanguage, const CString& pFileName)
{
	// Load File
	std::vector<CString> fileData = CString::loadToken(pFileName, "\n", true);
	if (fileData.size() == 0)
		return false;

	// Parse File
	std::vector<CString>::const_iterator cur, next;
	for (cur = fileData.begin(); cur != fileData.end(); ++cur)
	{
		if (cur->find("msgid") == 0)
		{
			CString msgId = cur->subString(7, cur->length() - 8);
			CString msgStr = "";
			bool isStr = false;

			++cur;
			while (cur != fileData.end())
			{
				// Make sure our string isn't empty.
				if (cur->isEmpty())
				{
					++cur;
					continue;
				}

				if ((*cur)[0] == '"' && (*cur)[cur->length() - 1] == '"')
				{
					CString str('\n');
					str.write(cur->subString(1, cur->length() - 2));
					(isStr ? msgStr.write(str) : msgId.write(str));
				}
				else if (cur->find("msgstr") == 0)
				{
					msgStr = cur->subString(8, cur->length() - 9);
					isStr = true;
				}
				else { --cur; break; }

				++cur;
			}

			mTranslationManager.add(pLanguage.text(), msgId.text(), msgStr.text());
		}

		if (cur == fileData.end())
			break;
	}

	return true;
}

CString TClient::TS_Translate(const CString& pLanguage, const CString& pKey)
{
	return mTranslationManager.translate(pLanguage.toLower().text(), pKey.text());
}

void TClient::TS_Reload()
{
	// Save Translations
	this->TS_Save();

	// Reset Translations
	mTranslationManager.reset();

	// Load Translation Folder
	CFileSystem translationFS(this);
	translationFS.addDir("translations", "*.po");

	// Load Each File
	std::map<CString, CString> *temp = translationFS.getFileList();
	for (auto i = temp->begin(); i != temp->end(); ++i)
		this->TS_Load(removeExtension(i->first), i->second);
}

void TClient::TS_Save()
{
	// Grab Translations
	std::map<std::string, STRMAP> *languages = mTranslationManager.getTranslationList();

	// Iterate each Language
	for (auto i = languages->begin(); i != languages->end(); ++i)
	{
		// Create Output
		CString output;

		// Iterate each Translation
		for (const auto & j : i->second)
		{
			output << "msgid ";
			std::vector<CString> sign = CString(j.first.c_str()).removeAll("\r").tokenize("\n");
			for (auto & s : sign)
				output << "\"" << s << "\"\r\n";
			output << "msgstr ";
			if (!j.second.empty())
			{
				std::vector<CString> lines = CString(j.second.c_str()).removeAll("\r").tokenize("\n");
				for (auto k = lines.begin(); k != lines.end(); ++k)
					output << "\"" << *k << "\"\r\n";
			}
			else output << "\"\"\r\n";

			output << "\r\n";
		}

		// Save File
		output.trimRight().save(getRunnerPath() << "translations/" << i->first.c_str() << ".po");
	}
}

