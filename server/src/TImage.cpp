
#include <unordered_map>
#include "main.h"
#include "TImage.h"
#include "TClient.h"

TImage::TImage(const CString& pName, TClient *theServer) {
	server = theServer;
	name = pName;
	real = pName.text() + pName.findl(CFileSystem::getPathSeparator()) + 1;


	imgcount = 1;
	if ( real.find(".png"))
		loaded = loadTexture(pName);
	else
		loaded = true;

	imageList.emplace(real.text(), this);
}

TImage::~TImage()
{
	TGameWindow::renderDestroyTexture(texture);

	auto imageIter = imageList.find(name.text());
	if (imageIter != imageList.end()) {
		delete imageIter->second;

		imageList.erase(imageIter);
	}
}

bool TImage::loadTexture(const CString& pImage) {
	//Load image at specified path
	GameTexture* loadedSurface = server->gameWindow->renderLoadImage(pImage.text());

	if( loadedSurface == nullptr ) {
		printf( "Unable to load image %s!\n", pImage.text() );
		return false;
	} else {
		TGameWindow::renderQueryTexture(loadedSurface, &width, &height);
		texture = loadedSurface;
	}

	return true;
}

void TImage::render(int pX, int pY, int pStartX, int pStartY, int pWidth, int pHeight, int r, int g, int b, int a) {
	if ( !texture || !loaded )
		return;

	auto srcRect = SDL_Rect({static_cast<Sint16>(pStartX),static_cast<Sint16>(pStartY), static_cast<Uint16>(pWidth), static_cast<Uint16>(pHeight)});
	auto dstRect = SDL_Rect({static_cast<Sint16>(pX),static_cast<Sint16>(pY), static_cast<Uint16>(pWidth), static_cast<Uint16>(pHeight)});
	if (a < SDL_ALPHA_OPAQUE) {
		TGameWindow::renderSetAlpha(texture, a);
	}
	server->gameWindow->renderBlit(texture, &srcRect, &dstRect);

}

TImage *TImage::find(const std::string& pName, TClient *theServer, bool addIfMissing) {
	if ( pName.empty()) return nullptr;

	auto imageIter = imageList.find(pName);
	if ( imageIter != imageList.end()) {
		return imageIter->second;
	}

	if (addIfMissing) {
		auto imageFile = theServer->getFileSystem(0)->find(pName);
		if ( imageFile != nullptr )
			return new TImage(imageFile, theServer);
	}

	return nullptr;
}

