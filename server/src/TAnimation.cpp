#include "TClient.h"
#include "TImage.h"
#include "main.h"
#include <SDL_image.h>
#include <unordered_map>
#include <TAccount.h>
#include <CGaniObjectStub.h>
#include "TAnimation.h"

/* Animations */
TAnimation::TAnimation(CString pName, TClient * theServer)
{
	server = theServer;
	name = pName;
	real = pName.text() + pName.findl(CFileSystem::getPathSeparator()) + 1;
	loaded = false;

	load();

	animations.emplace(real.text(), this);
}

TAnimation::~TAnimation()
{
	for (auto & i : animationSpriteList) {
		delete i.second;

		animationSpriteList.erase(i.first);
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
	int j = 0;
	for (const auto& line : lines)
	{
		auto words = line.tokenize(" ");

		if (words.empty())
			continue;

		if (words[0] == "CONTINUOUS" && words.size() == 2)
		{
			isContinuous = atoi(words[1].text());
		}
		else if (words[0] == "LOOP" && words.size() == 2)
		{
			isLoop = atoi(words[1].text());
		}
		else if (words[0] == "SETBACKTO" && words.size() == 2)
		{
			setBackTo = words[1];
		}
		else if (words[0] == "SINGLEDIRECTION" && words.size() == 2)
		{
			isSingleDir = atoi(words[1].text());
		}
		else if (words[0] == "SPRITE")
		{
			animationSpriteList.emplace(atoi(words[1].text()), new TAnimationSprite(atoi(words[1].text()), words[2].text(), atoi(words[3].text()), atoi(words[4].text()), atoi(words[5].text()), atoi(words[6].text()), words[7].text()));
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
				if (line.find("WAIT") == 0)
					continue;

				std::map<int, TAnimationAni*> anis;
				int k = 0;
				for (int i=0; i < words.size(); i++)
				{
					int sprite, x, y;
					sprite = atoi(words[i].text()); i++;
					x      = atoi(words[i].text()); i++;
					y      = atoi(words[i].text());
					anis.emplace(k, new TAnimationAni(animationSpriteList[sprite], x, y));
					k++;
				}
				animationAniList.emplace(j, anis);
				j++;
			} else {
				aniStarted = false;
			}
		}
	}

	max = (isSingleDir ? animationAniList.size() : animationAniList.size() / 4);
	loaded = true;

	return true;
}

void TAnimation::render(CGaniObjectStub * player, TClient * server, int pX, int pY, int pDir, int *pStep, float time)
{
	if ( animationAniList.empty() )
		return;

	if (currentWait >= wait) {
		currentWait = 0.0f;
		*pStep = (*pStep + 1) % max;
	} else {
		auto delta = time;
		currentWait += 0.025f;//(delta * 3);
	}

	if (*pStep >= max) *pStep = max-1;

	//*pStep = (isLoop ? (*pStep + 1) % max : (*pStep < max-1 ? *pStep + 1 : *pStep));
	auto list = animationAniList[(isSingleDir ? *pStep : (*pStep * 4) + pDir)];

	if (list.empty())
		return;

	for (auto & i : list) {
		if (i.second == nullptr) continue;
		i.second->render(player, server, pX, pY);
	}
}

TAnimation *TAnimation::find(const char *pName, TClient * theServer)
{
	auto aniIter = animations.find(pName);
	if (aniIter != animations.end()) {
		return aniIter->second;
	}
	auto ani = new TAnimation(theServer->getFileSystem(0)->find(pName), theServer);

	while (!ani->loaded) ;

	return ani;
}

TImage *TAnimation::findImage(char *pName, TClient * theServer)
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

TAnimationSprite::TAnimationSprite(int pSprite, std::string pImage, int pX, int pY, int pW, int pH, std::string desc)
{
	sprite = pSprite;
	img = pImage;
	x = pX;
	y = pY;
	w = pW;
	h = pH;
	description = desc;
}

TAnimationSprite::~TAnimationSprite()
{

}

void TAnimationSprite::render(CGaniObjectStub * player, TClient * server, int pX, int pY)
{
	TImage * image = nullptr;
	std::string tmpImg;

	if (img == "BODY") {
		tmpImg = player->getBodyImage().text();
	} else if (img == "HEAD") {
		tmpImg = player->getHeadImage().text();
	} else if (img == "SWORD") {
		tmpImg = player->getSwordImage().text();
	} else if (img == "SHIELD") {
		tmpImg = player->getShieldImage().text();
	} else if (img == "ATTR1") {
		tmpImg = "";//player->get;
	} else if (img == "SPRITES") {
		tmpImg = "sprites.png";
	} else {
		tmpImg = img;
	}

	image = TImage::find(tmpImg, server);

	if (image == nullptr)
		return;

	int alpha = SDL_ALPHA_OPAQUE;
	if (description == "shadow" ) {
		alpha = (SDL_ALPHA_OPAQUE/3)*2;
	}

	image->render(pX, pY, x, y, w, h, alpha);
}

TAnimationAni::TAnimationAni(TAnimationSprite *pImg, int pX, int pY)
{
	img = pImg;
	x = pX;
	y = pY;
}

void TAnimationAni::render(CGaniObjectStub * player, TClient * server, int pX, int pY)
{
	if (img == nullptr)
		return;


	img->render(player, server, pX + x, pY + y);
}