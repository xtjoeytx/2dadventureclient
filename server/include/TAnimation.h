//
// Created by marlon on 9/22/19.
//

#ifndef GS2EMU_TANIMATION_H
#define GS2EMU_TANIMATION_H

#include "TImage.h"

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

class TAnimation
{
	public:
		explicit TAnimation(CString pName, TServer * theServer);
		~TAnimation();

		bool loaded;
		CString name, real;

		bool load();
		void render(int pX, int pY, int pDir, int *pStep);

		static TAnimation *find(char *pName, TServer * theServer);
		TImage *findImage(char *pName, TServer * theServer);
	private:
		bool isloop, iscontinuous, issingledir;
		CString setbackto;
		std::unordered_map<std::string, TImage *> reallist;
		std::unordered_map<int, TAnimationSprite *> animationSpriteList;
		std::unordered_map<int, TAnimationAni *> animationAniList;
		TServer *server;

		int max;
		SDL_Thread *thread;
};



#include <SDL.h>
#include <SDL_image.h>
#include <CString.h>
#include "TImage.h"
#include "TServer.h"

#endif //GS2EMU_TANIMATION_H
