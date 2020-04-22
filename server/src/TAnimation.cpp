#include "TClient.h"
#include "TImage.h"
#include "main.h"
#include <SDL_image.h>
#include <unordered_map>
#include <utility>
#include <TAccount.h>
#include <CAnimationObjectStub.h>
#include "TAnimation.h"

/* Animations */
TAnimation::TAnimation(const CString& pName, TClient * client) : client(client)
{
	name = pName;
	real = pName.text() + pName.findl(CFileSystem::getPathSeparator()) + 1;
	loaded = false;

	if (load())
		animations.emplace(real.text(), this);
	else
		delete this;
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
	auto * fs = client->getFileSystem();
	auto file = fs->load(real);
	if (file == "")
		return false;

	auto lines = file.replaceAll('\r',"").tokenize("\n");
	bool aniStarted = false;
	int j = 0;
	for (const auto& line : lines)
	{
		auto words = line.tokenize(" ");

		if (words.empty())
			continue;

		if (words[0] == "CONTINUOUS")
		{
			isContinuous = true;
		}
		else if (words[0] == "LOOP")
		{
			isLoop = true;
		}
		else if (words[0] == "SETBACKTO" && words.size() > 1)
		{
			// This should make SETBACKTO accept animation names with spaces
			for(int i = 1; i < words.size(); i++){
				if(i > 1){
					setBackTo += " ";
				}
				setBackTo += words[i];
			}
		}
		else if (words[0] == "SINGLEDIRECTION")
		{
			isSingleDir = true;
		}
		else if (words[0] == "SPRITE")
		{
			TAnimationSprite *sprite;
			const char* desc = "";
			if (words.size() >= 8)
				desc = words[7].text();

			sprite = new TAnimationSprite(client, atoi(words[1].text()), words[2].text(), atoi(words[3].text()), atoi(words[4].text()), atoi(words[5].text()), atoi(words[6].text()), desc);

			animationSpriteList.emplace(atoi(words[1].text()), sprite);
		}
		else if (words[0] == "ANI" && words.size() == 1)
		{
			aniStarted = true;
		}
		else if (aniStarted) {
			if(line != "ANIEND")
			{
				if (line.find("PLAYSOUND") >= 0)
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
					anis.emplace(k, new TAnimationAni(client, animationSpriteList[sprite], x, y));
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

void TAnimation::render(CAnimationObjectStub * player, int pX, int pY, int pDir, int *pStep, float time)
{
	if ( animationAniList.empty() )
		return;

	if (currentWait >= wait) {
		currentWait = 0.0f;
		*pStep = (*pStep + 1) % max;
	} else {
		auto delta = time;
		currentWait +=(delta * 3); // 0.025f;//
	}

	if (*pStep >= max) *pStep = max-1;

	//*pStep = (isLoop ? (*pStep + 1) % max : (*pStep < max-1 ? *pStep + 1 : *pStep));
	auto list = animationAniList[(isSingleDir ? *pStep : (*pStep * 4) + pDir)];

	if (list.empty())
		return;

	for (auto & i : list) {
		if (i.second == nullptr) continue;
		i.second->render(player, pX, pY);
	}

	if (player->getChatMsg().length() > 0) {
		client->gameWindow->drawText(client->gameWindow->fontSmaller, player->getChatMsg().text(), pX+24+1, pY-34+1,{0,0,0}, CENTERED);
		client->gameWindow->drawText(client->gameWindow->fontSmaller, player->getChatMsg().text(), pX+24, pY-34,{255,255,255}, CENTERED);
	}

	client->gameWindow->drawText(client->gameWindow->fontSmaller, player->getNickname().text(), pX+24+1, pY+44+1,{0,0,0}, CENTERED);
	client->gameWindow->drawText(client->gameWindow->fontSmaller, player->getNickname().text(), pX+24, pY+44,{255,255,255}, CENTERED);
}

TAnimation *TAnimation::find(const char *fileName, TClient * client, bool addIfMissing)
{
	auto aniIter = animations.find(fileName);
	if (aniIter != animations.end()) {
		return aniIter->second;
	}

	if (addIfMissing) {
		auto aniFile = client->getFileSystem(0)->find(fileName);
		if ( aniFile != nullptr )
			return new TAnimation(aniFile, client);
	}

	return nullptr;
}

TAnimationSprite::TAnimationSprite(TClient * client, int pSprite, std::string pImage, int pX, int pY, int pW, int pH, std::string desc) : client(client), sprite(pSprite), img(std::move(pImage)), x(pX), y(pY), w(pW), h(pH), description(std::move(desc)) {}

TAnimationSprite::~TAnimationSprite() = default;

void TAnimationSprite::render(CAnimationObjectStub * player, int pX, int pY)
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

	image = TImage::find(tmpImg.c_str(), client);

	if (image == nullptr)
		return;

	int alpha = SDL_ALPHA_OPAQUE;
	if (description == "shadow" ) {
		alpha = (SDL_ALPHA_OPAQUE/3)*2;
	}

	image->render(pX, pY, x, y, w, h, alpha);
}

TAnimationAni::TAnimationAni(TClient * client, TAnimationSprite *pImg, int pX, int pY) : client(client), img(pImg), x(pX), y(pY) { }

void TAnimationAni::render(CAnimationObjectStub * player, int pX, int pY)
{
	if (img == nullptr)
		return;

	img->render(player, pX + x, pY + y);
}