#ifndef GS2EMU_CRUNNERSTUB_H
#define GS2EMU_CRUNNERSTUB_H

#include <vector>
#include <map>
#include <unordered_map>
#include <set>
#include <thread>
#include <chrono>
#include <climits>

#ifdef V8NPCSERVER
#include <string>
#include <unordered_set>
#endif

#include "IEnums.h"
#include "CString.h"
#include "Timer.h"
#include "CLog.h"
#include "CFileSystem.h"
#include "CSettings.h"
//#include "CSocket.h"
#include "CAnimationObjectStub.h"
#include "CTranslationManager.h"
#include "CWordFilter.h"

#ifdef V8NPCSERVER
#include "CScriptEngine.h"
#endif

class TPlayer;

class TLevel;

class TNPC;

class TMap;

class TWeapon;
class ScriptRunError;
class IScriptFunction;

class CRunnerStub {
public:
	virtual void operator()() = 0;

	virtual void cleanup() = 0;

	virtual void restart() = 0;

	virtual TNPC *addNPC(const CString &pImage, const CString &pScript, float pX, float pY, TLevel *pLevel, bool pLevelNPC, bool sendToPlayers = false) = 0;

	virtual unsigned int getNWTime() const = 0;

	virtual std::unordered_map<std::string, std::string> *getClassList() = 0;

	virtual std::unordered_map<std::string, TNPC *> *getNPCNameList() = 0;

	virtual std::unordered_map<std::string, CString> *getServerFlags() = 0;

	virtual std::map<CString, TWeapon *> *getWeaponList() = 0;

	virtual std::vector<TPlayer *> *getPlayerList() = 0;

	virtual std::vector<TNPC *> *getNPCList() = 0;

	virtual std::vector<TLevel *> *getLevelList() = 0;

	virtual std::vector<TMap *> *getMapList() = 0;

	virtual std::vector<CString> *getStatusList() = 0;

	virtual std::vector<CString> *getAllowedVersions() = 0;

	virtual std::map<CString, std::map<CString, TLevel *> > *getGroupLevels() = 0;

	virtual CFileSystem* getFileSystemByType(CString& type) = 0;

	virtual CString getFlag(const std::string& pFlagName) = 0;

	virtual CString getRunnerPath() = 0;

	virtual TLevel *getLevel(const CString &pLevel) = 0;

	virtual std::string getClass(const std::string &className) const = 0;

	virtual TMap* getMap(const CString& name) const = 0;

	virtual TMap* getMap(const TLevel* pLevel) const = 0;

	virtual TNPC* getNPC(const unsigned int id) const = 0;

	virtual TPlayer* getPlayer(const unsigned short id, int type) const = 0;

	virtual TPlayer* getPlayer(const CString& account, int type) const = 0;

	virtual void logToFile(const std::string& fileName, const std::string& message) = 0;

	virtual bool deleteFlag(const std::string& pFlagName, bool pSendToPlayers = true) = 0;

	virtual bool setFlag(CString pFlag, bool pSendToPlayers = true) = 0;

	virtual bool setFlag(const std::string& pFlagName, const CString& pFlagValue, bool pSendToPlayers = true) = 0;

	// Admin chat functions
	virtual inline void sendToRC(const CString& pMessage, TPlayer *pPlayer = 0) const = 0;

	virtual inline void sendToNC(const CString& pMessage, TPlayer *pPlayer = 0) const = 0;

#ifdef V8NPCSERVER

	virtual void saveNpcs() = 0;

	virtual std::vector<std::pair<double, std::string>> calculateNpcStats() = 0;

	virtual void reportScriptException(const ScriptRunError &error) = 0;

	virtual void reportScriptException(const std::string &error_message) = 0;

	virtual TPlayer *getNPCServer() const = 0;

	virtual void assignNPCName(TNPC *npc, const std::string &name) = 0;

	virtual void removeNPCName(TNPC *npc) = 0;

	virtual TNPC *getNPCByName(const std::string &name) const = 0;

	virtual TNPC *addServerNpc(int npcId, float pX, float pY, TLevel *pLevel, bool sendToPlayers = false) = 0;

	virtual void handlePM(TPlayer *player, const CString &message) = 0;

	virtual void setPMFunction(TNPC *npc, IScriptFunction *function = nullptr) = 0;

#endif
};

#endif //GS2EMU_CRUNNERSTUB_H
