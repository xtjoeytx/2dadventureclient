#ifndef TIMAGE_H
#define TIMAGE_H

#include <SDL.h>
#include <CString.h>
#include "TGameWindow.h"
#include "TClient.h"


class TImage
{
	public:
		explicit TImage(const CString& pName, TClient * client);
		~TImage();

		CString name, real;

		bool loadTexture(const char* fileName);

		inline void render(int pX, int pY);
		inline void render(int pX, int pY, int pStartX, int pStartY, int pWidth, int pHeight, int alpha);
		void render(int pX, int pY, int pStartX, int pStartY, int pWidth, int pHeight, int r, int g, int b, int a = SDL_ALPHA_OPAQUE);

		static TImage *find(const char* fileName, TClient * client, bool addIfMissing = false);

		[[nodiscard]] int getWidth() const { return width; }
		[[nodiscard]] int getHeight() const { return height; }

	private:
		bool loaded;
		int imgcount, fullwidth{}, fullheight{}, height{}, width{};

		GameTexture * texture{};

		TClient *client;
};

inline void TImage::render(int pX, int pY)
{
	render(pX, pY, 0, 0, width, height, SDL_ALPHA_OPAQUE);
}

inline void TImage::render(int pX, int pY, int pStartX, int pStartY, int pWidth, int pHeight, int alpha = SDL_ALPHA_OPAQUE)
{
	render(pX, pY, pStartX, pStartY, pWidth, pHeight, 255, 255, 255, alpha);
}

#endif
