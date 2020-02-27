#include "IDebug.h"
#include <thread>
#include <atomic>
#include <functional>
#include <csignal>
#include <cstdlib>
#include <map>
#include <SDL.h>

#include "main.h"
#include "IConfig.h"
#include "CString.h"
#include "IUtil.h"
#include "CLog.h"
//#include "CSocket.h"
#include "TClient.h"


// Linux specific stuff.
#if !(defined(_WIN32) || defined(_WIN64))
	#include <unistd.h>
	#ifndef SIGBREAK
		#define SIGBREAK SIGQUIT
	#endif
#else
#include <minwindef.h>
#include <processenv.h>
#endif

// Function pointer for signal handling.
typedef void (*sighandler_t)(int);

std::map<CString, TClient*> serverList;
#ifndef __AMIGA__
std::map<CString, std::thread*> serverThreads;
#endif
CLog clientLog("startuplog.txt");
CString overrideServer;

// Home path of the gserver.
CString homePath;
static void getBasePath();

#ifndef __AMIGA__
std::atomic_bool shutdownProgram{ false };
#else
bool shutdownProgram = false;
#endif

int main(int argc, char* argv[])
{
	if (parseArgs(argc, argv))
		return 1;

#if (defined(_WIN32) || defined(_WIN64) || defined(WIN32) || defined(WIN64)) && defined(_MSC_VER)
#if defined(DEBUG) || defined(_DEBUG)
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );
#endif
#endif

	{
		// Shut down the server if we get a kill signal.
		signal(SIGINT, (sighandler_t)shutdownClient);
		signal(SIGTERM, (sighandler_t)shutdownClient);
		signal(SIGBREAK, (sighandler_t)shutdownClient);
		signal(SIGABRT, (sighandler_t)shutdownClient);

		// Seed the random number generator with the current time.
		srand((unsigned int)time(nullptr));

		// Grab the base path to the server executable.
		getBasePath();

		// Program announcements.
		clientLog.out( "%s v%s\n", GSERVER_APPNAME, GSERVER_VERSION);
		clientLog.out("Programmed by %s.\n\n", GSERVER_CREDITS);

		SDL_Init(SDL_INIT_EVERYTHING);
		//SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY, SDL_DEFAULT_REPEAT_INTERVAL);

		// Load Server Settings
		if (overrideServer.isEmpty())
		{
#ifndef __AMIGA__
			clientLog.out(":: Loading servers.txt... ");
			CSettings serversettings(CString(homepath) << "servers.txt");
			if (!serversettings.isOpened())
			{
				clientLog.append("FAILED!\n");
				return ERR_SETTINGS;
			}
			clientLog.append("success\n");

			// Make sure we actually have a server.
			if (serversettings.getInt("servercount", 0) == 0)
			{
				clientLog.out("** [Error] Incorrect settings.txt file.  servercount not found.\n");
				return ERR_SETTINGS;
			}

			// Load servers.

			for (int i = 1; i <= serversettings.getInt("servercount"); ++i)
			{
				CString name = serversettings.getStr(CString() << "server_" << CString(i), "default");
#else
				CString name = "default";
#endif

				auto* client = new TClient(name);

#ifndef __AMIGA__
				// Make sure doubles don't exist.
				if (serverList.find(name) != serverList.end())
				{
					clientLog.out("-- [WARNING] Server %s already found, deleting old server.\n", name.text());
					delete serverList[name];
				}

				// See if an override was specified.
				CString serverip = serversettings.getStr(CString() << "server_" << CString(i) << "_ip");
				CString serverport = serversettings.getStr(CString() << "server_" << CString(i) << "_port");
				CString localip = serversettings.getStr(CString() << "server_" << CString(i) << "_localip");
				CString serverinterface = serversettings.getStr(CString() << "server_" << CString(i) << "_interface");

				// Initialize the server.
				clientLog.out(":: Starting server: %s.\n", name.text());

				if (server->init(serverip, serverport, localip, serverinterface) != 0)
				{
					clientLog.out("** [Error] Failed to start server: %s\n", name.text());
					delete server;
					continue;
				}
				serverList[name] = server;
#else
				if ( client->init() != 0)
				{
					clientLog.out("** [Error] Failed to start...\n");
					delete client;
				}
#endif

				(*client)();
#ifndef __AMIGA__
				// Put the server in its own thread.
				serverThreads[name] = new std::thread(std::ref(*server));
			}
#endif
		}
#ifndef __AMIGA__
		else {
			TClient *server = new TClient(overrideServer);
			serverlog.out(":: Starting server: %s.\n", overrideServer.text());
			if ( server->init() != 0 ) {
				serverlog.out("** [Error] Failed to start server: %s\n", overrideServer.text());
				delete server;
				return 1;
			}
			serverList[overrideServer] = server;


			// Put the server in its own thread.
			serverThreads[overrideServer] = new std::thread(std::ref(*server));

		}
#endif
		// Announce that the program is now running.
		clientLog.out(":: Program started.\n");
	#if defined(WIN32) || defined(WIN64)
		clientLog.out(":: Press CTRL+C to close the program.  DO NOT CLICK THE X, you will LOSE data!\n");
	#endif

#ifndef __AMIGA__
		// Wait on each thread to end.
		// Once all threads have ended, the program has terminated.
		for (std::map<CString, std::thread*>::iterator i = serverThreads.begin(); i != serverThreads.end();)
		{
			std::thread* t = i->second;
			if (t == 0) serverThreads.erase(i++);
			else
			{
				t->join();
				++i;
			}
		}

		// Delete all the servers.
		for (auto i = serverList.begin(); i != serverList.end(); )
		{
			delete i->second;
			serverList.erase(i++);
		}

		// Destroy the sockets.
		CSocket::socketSystemDestroy();
#endif
	}

	return ERR_SUCCESS;
}

/*
	Extra-Cool Functions :D
*/

bool parseArgs(int argc, char* argv[])
{
	std::vector<CString> args;
	for (int i = 0; i < argc; ++i)
		args.emplace_back(argv[i]);

	for (auto i = args.begin(); i != args.end(); ++i)
	{
		if ((*i).find("--") == 0)
		{
			CString key((*i).subString(2));
			if (key == "help")
			{
				printHelp(args[0].text());
				return true;
			}
			else if (key == "server")
			{
				++i;
				if (i == args.end())
				{
					printHelp(args[0].text());
					return true;
				}
				overrideServer = *i;
			}
		}
		else if ((*i)[0] == '-')
		{
			for (int j = 1; j < (*i).length(); ++j)
			{
				if ((*i)[j] == 'h')
				{
					printHelp(args[0].text());
					return true;
				}
				if ((*i)[j] == 's')
				{
					++i;
					if (i == args.end())
					{
						printHelp(args[0].text());
						return true;
					}
					overrideServer = *i;
				}
			}
		}
	}
	return false;
}

void printHelp(const char* pname)
{
	clientLog.out("%s v%s\n", GSERVER_APPNAME, GSERVER_VERSION);
	clientLog.out("Programmed by %s.\n\n", GSERVER_CREDITS);
	clientLog.out("USAGE: %s [options]\n\n", pname);
	clientLog.out("Commands:\n\n");
	clientLog.out(" -h, --help\t\tPrints out this help text.\n");
	clientLog.out(" -s, --server DIRECTORY\tDirectory that contains the server.\n");
	clientLog.out("\n");
}

const CString getHomePath()
{
	return homePath;
}

void shutdownClient(int sig)
{
	clientLog.out(":: The server is now shutting down...\n-------------------------------------\n\n");

	shutdownProgram = true;
}

void getBasePath()
{
#if defined(_WIN32) || defined(_WIN64)
	// Get the path.
	char path[MAX_PATH];
	GetCurrentDirectoryA(MAX_PATH,path);

	// Find the program exe and remove it from the path.
	// Assign the path to homepath.
	homePath = path;
	homePath += "\\";
	int pos = homePath.findl('\\');
	if (pos == -1) homePath.clear();
	else if (pos != (homePath.length() - 1))
		homePath.removeI(++pos, homePath.length());
#else
	// Get the path to the program.
	char path[260];
	memset((void*)path, 0, 260);
	readlink("/proc/self/exe", path, sizeof(path));

	// Assign the path to homepath.
	char* end = strrchr(path, '/');
	if (end != nullptr) {
		end++;
		if (end != nullptr) *end = '\0';
		homePath = path;
	}
#endif
}
