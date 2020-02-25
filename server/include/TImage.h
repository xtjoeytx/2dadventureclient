#ifndef TIMAGE_H
#define TIMAGE_H

#include <SDL.h>
#include <CString.h>
#include "TGameWindow.h"
#include "TClient.h"


class TImage
{
	public:
		explicit TImage(const CString& pName, TClient * theServer);
		~TImage();

		CString name, real;

		bool countChange(int pCount);
		bool loadTexture(const CString& pImage);
		inline void render(int pX, int pY);
		inline void render(int pX, int pY, int pStartX, int pStartY, int pWidth, int pHeight, int alpha);
		inline void render(int pX, int pY, float r, float g, float b, float a);
		void render(int pX, int pY, int pStartX, int pStartY, int pWidth, int pHeight, float r, float g, float b, float a);

		static TImage *find(const std::string& pName, TClient * theServer, bool addIfMissing = false);

		int getWidth() const { return width; }
		int getHeight() const { return height; }

	private:
		bool loaded;
		int imgcount, fullwidth, fullheight, height, width;

		GameTexture * texture;

		TClient *server;
};

inline void TImage::render(int pX, int pY)
{
	render(pX, pY, 0, 0, width, height, 1.0f, 1.0f, 1.0f, 1.0f);
}

inline void TImage::render(int pX, int pY, int pStartX, int pStartY, int pWidth, int pHeight, int alpha = SDL_ALPHA_OPAQUE)
{
	render(pX, pY, pStartX, pStartY, pWidth, pHeight, 1.0f, 1.0f, 1.0f, float(alpha));
}

inline void TImage::render(int pX, int pY, float r, float g, float b, float a)
{
	render(pX, pY, 0, 0, width, height, r, g, b, a);
}

#endif
