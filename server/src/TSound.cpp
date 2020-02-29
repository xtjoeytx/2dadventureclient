
#include <unordered_map>
#include "main.h"
#include "TSound.h"
#include "TClient.h"

TSound::TSound(const CString& pName, TClient *client) : client(client), audio(nullptr) {
	name = pName;
	real = pName.text() + pName.findl(CFileSystem::getPathSeparator()) + 1;

	if ( real.find(".wav") > 0)
		loaded = loadSound(pName.text());
	else
		loaded = true;

	audioList.emplace(real.text(), this);
}

TSound::~TSound()
{
	TGameWindow::destroySound(audio);

	auto audioIter = audioList.find(name.text());
	if (audioIter != audioList.end()) {
		delete audioIter->second;

		audioList.erase(audioIter);
	}
}

bool TSound::loadSound(const char* fileName) {
	//Load sound at specified path
	Mix_Chunk * loadedSound = TGameWindow::loadSound(fileName);


	if( loadedSound == nullptr ) {
		printf( "Unable to load sound %s!\n", fileName );
		return false;
	} else {
		Mix_VolumeChunk( loadedSound, MIX_MAX_VOLUME );
		audio = loadedSound;
	}

	return true;
}

TSound *TSound::find(const char* fileName, TClient *client, bool addIfMissing) {
	if ( strlen(fileName) == 0 ) return nullptr;

	auto audioIter = audioList.find(fileName);
	if ( audioIter != audioList.end()) {
		return audioIter->second;
	}

	if (addIfMissing) {
		auto soundFile = client->getFileSystem(0)->find(fileName);
		if ( soundFile != nullptr )
			return new TSound(soundFile, client);
	}

	return nullptr;
}

void TSound::playSound(int x1, int y1, int x2, int y2) {
	if ( !audio || !loaded )
		return;

	auto deltaX = x2 - x1;
	auto deltaY = y2 - y1;
	auto distance = sqrt( deltaX*deltaX + deltaY*deltaY );

	Uint8 distance2 = ((distance - 0) / (64 - 0))
	* (255 - 1) + 1;
	auto rad = atan2(deltaY, deltaX);
	int chan = TGameWindow::getFreeChannel();

	int deg = int(round(rad)) * (180 / M_PI);

	Sint16 angle = deg % 360;

	Mix_SetPosition(chan, angle, distance2);

	Mix_PlayChannel(chan, audio, 0);
	//client->gameWindow->renderBlit(texture, &srcRect, &dstRect);

}
