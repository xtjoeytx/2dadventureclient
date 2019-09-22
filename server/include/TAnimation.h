//
// Created by marlon on 9/22/19.
//

#ifndef GS2EMU_TANIMATION_H
#define GS2EMU_TANIMATION_H

#include "TImage.h"

class TAnimation
{
	public:
		explicit TAnimation(CString pName);
		~TAnimation();

		bool loaded;
		CString name, real;

		bool load();
		void render(int pX, int pY, int pDir, int *pStep);

		static TAnimation *find(char *pName);
		TImage *findImage(char *pName);
	private:
		bool isloop, iscontinuous, issingledir;
		CString setbackto;
		CString animations, imglist, reallist;

		int max;
		SDL_Thread *thread;
};

class TAnimationSprite
{
	public:
		TAnimationSprite(int pSprite, TImage *pImage, int pX, int pY, int pW, int pH);
		~TAnimationSprite();

		inline void render(int pX, int pY);

	private:
		TImage *img;
		int sprite, x, y, w, h;
};

class TAnimationAni
{
	public:
		TAnimationAni(TAnimationSprite *pImg, int pX, int pY);

		int x, y;
		TAnimationSprite *img;

		inline void render(int pX, int pY);
};

#include <SDL.h>
#include <SDL_image.h>
#include <CString.h>
#include "TImage.h"
#include "TServer.h"

#endif //GS2EMU_TANIMATION_H
