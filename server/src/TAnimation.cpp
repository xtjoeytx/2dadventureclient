//
// Created by marlon on 9/22/19.
//

#include "TServer.h"
#include "TImage.h"
#include "main.h"
#include <SDL_image.h>
#include <unordered_map>
#include <TAccount.h>
#include "TAnimation.h"

/* Animations */
TAnimation::TAnimation(CString pName, TServer * theServer)
{
	server = theServer;
	name = pName;
	real = pName.text() + pName.findl(CFileSystem::getPathSeparator()) + 1;
	load();

	animations.emplace(real.text(), this);
}

TAnimation::~TAnimation()
{
	for (int i = 0; i < animationSpriteList.size();i++) {
		delete animationSpriteList[i];

		animationSpriteList.erase(animationSpriteList.begin() + i);
	}

	auto list = animations.find(name.text());
	if (list != animations.end()) {

		delete list->second;

		animations.erase(list);
	}

	animationSpriteList.clear();
	animationAniList.clear();
}

bool TAnimation::load()
{
	auto * fs = server->getFileSystem();
	auto file = fs->load(real);
	if (file == "")
	{
		delete this;
		return false;
	}

	auto lines = file.replaceAll('\r',"").tokenize("\n");
	bool aniStarted = false;

	for (const auto& line : lines)
	{
		auto words = line.tokenize(" ");

		if (words.empty())
			continue;

		if (words[0] == "CONTINUOUS" && words.size() == 2)
		{
			iscontinuous = atoi(words[1].text());
		}
		else if (words[0] == "LOOP" && words.size() == 2)
		{
			isloop = atoi(words[1].text());
		}
		else if (words[0] == "SETBACKTO" && words.size() == 2)
		{
			setbackto = words[1];
		}
		else if (words[0] == "SINGLEDIRECTION" && words.size() == 2)
		{
			issingledir = atoi(words[1].text());
		}
		else if (words[0] == "SPRITE")
		{
			animationSpriteList.push_back(new TAnimationSprite(atoi(words[1].text()), words[2].text(), atoi(words[3].text()), atoi(words[4].text()), atoi(words[5].text()), atoi(words[6].text())));
		}
		else if (words[0] == "ANI" && words.size() == 1)
		{
			aniStarted = true;
		}
		else if (aniStarted) {
			if(line != "ANIEND")
			{
				if (line.find("PLAYSOUND") == 0)
					continue;

				for (int i=0; i < words.size(); i++)
				{
					int sprite, x, y;
					sprite = atoi(words[i].text()); i++;
					x      = atoi(words[i].text()); i++;
					y      = atoi(words[i].text());
					animationAniList.push_back(new TAnimationAni(animationSpriteList[sprite], x, y));
				}

			} else {
				aniStarted = false;
			}
		}
	}

	max = (issingledir ? animationAniList.size() : animationAniList.size() / 4);


	return true;
}

void TAnimation::render(TPlayer * player, TServer * server, int pX, int pY, int pDir, int pStep)
{
	if ( animationAniList.empty() )
		return;

	pStep = (pStep + 1) % max;

	//*pStep = (isloop ? (*pStep + 1) % max : (*pStep < max-1 ? *pStep + 1 : *pStep));
	TAnimationAni *list = animationAniList[(issingledir ? pStep : pStep * 4 + pDir)];

	if (list == nullptr)
		return;

	list->render(player, server, pX, pY);
}

TAnimation *TAnimation::find(const char *pName, TServer * theServer)
{
	auto aniIter = animations.find(pName);
	if (aniIter != animations.end()) {
		return aniIter->second;
	}

	return new TAnimation(theServer->getFileSystem(0)->find(pName), theServer);
}

TImage *TAnimation::findImage(char *pName, TServer * theServer)
{
	auto imageIter = imageList.find(pName);
	if ( imageIter != imageList.end()) {
		return imageIter->second;
	}

	auto * img = TImage::find(pName, theServer);
	if (img != nullptr) {
		imageList.emplace(pName, img);
		return img;
	}

	return nullptr;
}

TAnimationSprite::TAnimationSprite(int pSprite, std::string pImage, int pX, int pY, int pW, int pH)
{
	sprite = pSprite;
	img = pImage;
	x = pX;
	y = pY;
	w = pW;
	h = pH;
}

TAnimationSprite::~TAnimationSprite()
{

}

void TAnimationSprite::render(TPlayer * player, TServer * server, int pX, int pY)
{
	TImage * image = nullptr;
	if (img == "BODY")
		image = TImage::find(player->getProp(PLPROP_BODYIMG).text()+1, server);

	if (image == nullptr)
		return;

	image->render(pX, pY, x, y, w, h);
}

TAnimationAni::TAnimationAni(TAnimationSprite *pImg, int pX, int pY)
{
	img = pImg;
	x = pX;
	y = pY;
}

void TAnimationAni::render(TPlayer * player, TServer * server, int pX, int pY)
{
	if (img == nullptr)
		return;


	img->render(player, server, pX + x, pY + y);
}