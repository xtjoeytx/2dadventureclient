//
// Created by marlon on 9/22/19.
//

#include "TServer.h"
#include "TImage.h"
#include "main.h"
#include <SDL_image.h>
#include <unordered_map>
#include "TAnimation.h"

/* Animations */
TAnimation::TAnimation(CString pName)
{
	name = pName;
	real = pName.text() + pName.findl('\\') + 1;
	load();

	aniList.add(this);
}

TAnimation::~TAnimation()
{
	aniList.remove(this);

	for (int i = 0; i < imglist.count(); i++)
		delete (TAnimationSprite *)imglist[i];

	for (int i = 0; i < animations.count(); i++)
	{
		CList *list = (CList *)animations[i];
		for (int i = 0; i < list->count(); i++)
			delete (TAnimationAni *)list->item(i);
		delete list;
	}

	imglist.clear();
	animations.clear();
}

bool TAnimation::load()
{
	CBuffer pFile;
	if (!pFile.load(name.text()))
	{
		delete this;
		return false;
	}

	char buffer[65535];
	unsigned long len = sizeof(buffer);
	int error = uncompress((Bytef *)buffer, (uLongf *)&len, (const Bytef *)pFile.text(), pFile.length());
	if (error != Z_OK)
	{
		printf("Error Decompressing\n");
		return false;
	}

	CStringList lines;
	lines.load(buffer, "\r\n");

	if (lines[0] == "PANI001")
	{
		for (int i = 1; i < lines.count(); i++)
		{
			CStringList words;
			words.load(lines[i].text(), " ");
			if (words.count() < 1)
				continue;

			if (words[0] == "CONTINUOUS" && words.count() == 2)
			{
				iscontinuous = atoi(words[1].text());
			}
				else if (words[0] == "LOOP" && words.count() == 2)
			{
				isloop = atoi(words[1].text());
			}
				else if (words[0] == "SETBACKTO" && words.count() == 2)
			{
				setbackto = words[1];
			}
				else if (words[0] == "SINGLEDIRECTION" && words.count() == 2)
			{
				issingledir = atoi(words[1].text());
			}
				else if (words[0] == "SPRITE" && words.count() == 7)
			{
				imglist.add(new TAnimationSprite(atoi(words[1].text()), findImage(words[2].text()), atoi(words[3].text()), atoi(words[4].text()), atoi(words[5].text()), atoi(words[6].text())));
			}
				else if (words[0] == "ANI" && words.count() == 1)
			{
				for (i++; i < lines.count() && lines[i] != "ANIEND"; i++)
				{
					if (lines[i].find("PLAYSOUND") == 0)
						continue;

					CList *list = new CList();
					animations.add(list);
					words.load(lines[i].text(), " ");
					for (int i = 0; i < words.count(); i++)
					{
						int sprite, x, y;
						sprite = atoi(words[i].text()); i++;
						x      = atoi(words[i].text()); i++;
						y      = atoi(words[i].text());
						list->add(new TAnimationAni((TAnimationSprite *)imglist[sprite], x, y));
					}

				}
			}
		}

		max = (issingledir ? animations.count() : animations.count() / 4);
	}
		else
	{
		return false;
	}

	return true;
}

void TAnimation::render(int pX, int pY, int pDir, int *pStep)
{
	if (animations.count() < 1)
		return;

	*pStep = (*pStep + 1) % max;

	//*pStep = (isloop ? (*pStep + 1) % max : (*pStep < max-1 ? *pStep + 1 : *pStep));
	CList *list = (CList *)animations[(issingledir ? *pStep : *pStep * 4 + pDir)];

	if (list == NULL)
		return;

	for (int i = 0; i < list->count(); i++)
	{
		TAnimationAni *img = (TAnimationAni *)list->item(i);
		if (img == NULL)
			continue;

		img->render(pX, pY);
	}
}

TAnimation *TAnimation::find(char *pName)
{
	for (int i = 0; i < aniList.count(); i++)
	{
		TAnimation *ani = (TAnimation *)aniList[i];
		if (ani->real == pName)
			return ani;
	}

	return new TAnimation(TFile::find(pName));
}

TImage *TAnimation::findImage(char *pName)
{
	for (int i = 0; i < reallist.count(); i++)
	{
		TImage *img = (TImage *)reallist[i];
		if (img->real == pName)
			return img;
	}

	reallist.add(TImage::find(pName));
	return (TImage *)reallist[reallist.count() - 1];
}

TAnimationSprite::TAnimationSprite(int pSprite, TImage *pImage, int pX, int pY, int pW, int pH)
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
	if (img == NULL)
		return;

	img->countChange(-1);
}

void TAnimationSprite::render(int pX, int pY)
{
	if (img == NULL)
		return;

	img->render(pX, pY, x, y, w, h);
}

TAnimationAni::TAnimationAni(TAnimationSprite *pImg, int pX, int pY)
{
	img = pImg;
	x = pX;
	y = pY;
}

void TAnimationAni::render(int pX, int pY)
{
	if (img == NULL)
		return;

	img->render(pX + x, pY + y);
}