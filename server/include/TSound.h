#ifndef TSOUND_H
#define TSOUND_H

#include <SDL.h>
#include <CString.h>
#include "TGameWindow.h"
#include "TClient.h"


class TSound
{
	public:
		explicit TSound(const CString& fileName, TClient * client);
		~TSound();

		CString name, real;

		bool loadSound(const char* fileName);

		static TSound *find(const char* fileName, TClient * client, bool addIfMissing = false);

		void playSound(int x1, int y1, int x2, int y2);
	private:
		bool loaded;

		Mix_Chunk * audio{};

		TClient *client;
};

#endif
