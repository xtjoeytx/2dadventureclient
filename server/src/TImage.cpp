
#include <unordered_map>

#include <SDL.h>
#include <TGameWindow.h>
#include "main.h"
#include "TImage.h"
#include "TServer.h"

TImage::TImage(CString pName, TServer * theServer)
{
	server = theServer;
	name = pName;
	real = pName.text() + pName.findl(CFileSystem::getPathSeparator()) + 1;
	imageList.emplace(name.text(),this);

	imgcount = 1;
	if (real.find(".png"))
		loaded = loadTexture(pName);
	else
		loaded = true;
}

TImage::~TImage()
{
	//SDL_FreeSurface(texture);

	auto imageIter = imageList.find(name.text());
	if (imageIter != imageList.end()) {
		delete imageIter->second;

		imageList.erase(imageIter);
	}
}

bool TImage::loadTexture(CString pImage) {
	//The final texture
	SDL_Texture* newTexture = nullptr;

	//Load image at specified path
	SDL_Surface* loadedSurface = IMG_Load( pImage.text() );
	if( loadedSurface == nullptr ) {
		printf( "Unable to load image %s! SDL_image Error: %s\n", pImage.text(), IMG_GetError() );
		return false;
	} else {
		//Create texture from surface pixels
		newTexture = SDL_CreateTextureFromSurface( server->gameWindow->renderer, loadedSurface );
		if( newTexture == nullptr ) {
			printf( "Unable to create texture from %s! SDL Error: %s\n", pImage.text(), SDL_GetError() );
		}

		width = loadedSurface->w;
		height = loadedSurface->h;
		texture = newTexture;

		//Get rid of old loaded surface
		SDL_FreeSurface( loadedSurface );
	}


	return true;
}

void TImage::render(int pX, int pY, int pStartX, int pStartY, int pWidth, int pHeight, float r, float g, float b, float a) {
	if ( !texture || !loaded )
		return;

	auto srcRect = SDL_Rect({static_cast<Sint16>(pStartX),static_cast<Sint16>(pStartY), static_cast<Uint16>(pWidth), static_cast<Uint16>(pHeight)});
	auto dstRect = SDL_Rect({static_cast<Sint16>(pX),static_cast<Sint16>(pY), static_cast<Uint16>(pWidth), static_cast<Uint16>(pHeight)});
	if (a < 255) {
		SDL_SetTextureAlphaMod(texture, a);
	}
	SDL_RenderCopy( server->gameWindow->renderer, texture, &srcRect, &dstRect );
}

TImage *TImage::find(std::string pName, TServer * theServer)
{
	auto imageIter = imageList.find(pName);
	if (imageIter != imageList.end()) {
		return imageIter->second;
	}

	auto imageFile = theServer->getFileSystem(0)->find(pName);
	if (imageFile != nullptr) {
		return new TImage(imageFile, theServer);
	}

	return nullptr;
}

