
#include <unordered_map>

#include <SDL_image.h>
#include "main.h"
#include "TImage.h"
#include "TServer.h"

TImage::TImage(CString pName, TServer * theServer)
{
	server = theServer;
	name = pName;
	real = pName.text() + pName.findl('\\') + 1;
	imageList.emplace(name.text(),this);

	imgcount = 1;
	loaded = loadTexture(pName);
}

TImage::~TImage()
{
	SDL_FreeSurface(texture);

	auto imageIter = imageList.find(name.text());
	if (imageIter != imageList.end()) {
		delete imageIter->second;

		imageList.erase(imageIter);
	}
}

bool TImage::countChange(int pCount)
{
	imgcount += pCount;
	if (imgcount <= 0)
	{
		delete this;
		return false;
	}

	return true;
}

bool TImage::loadTexture(CString pImage) {
	texture = IMG_Load(pImage.text());
	if (!texture)
		return false;

	width = texture->w;
	height = texture->h;

	return true;
}

void TImage::render(int pX, int pY, int pStartX, int pStartY, int pWidth, int pHeight, float r, float g, float b, float a) {
	if ( !texture || !loaded )
		return;

	auto srcRect = SDL_Rect({static_cast<Sint16>(pX+pStartX),static_cast<Sint16>(pY+pStartY), static_cast<Uint16>(pWidth), static_cast<Uint16>(pHeight)});

	SDL_BlitSurface(texture, &srcRect, server->camera, &srcRect);
}

TImage *TImage::find(char *pName, TServer * theServer)
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

