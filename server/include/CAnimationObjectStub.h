#ifndef GS2EMU_CGANIOBJECTSTUB_H
#define GS2EMU_CGANIOBJECTSTUB_H

#ifdef V8NPCSERVER
#include "CScriptEngine.h"
#endif

#include <CString.h>
#include "TLevel.h"


class CAnimationObjectStub {
public:
	virtual float getX() const = 0;

	virtual float getY() const = 0;

	virtual int getPixelX() const = 0;

	virtual int getPixelY() const = 0;

	virtual const CString &getBodyImage() const = 0;

	virtual const CString &getHeadImage() const = 0;

	virtual const CString &getShieldImage() const = 0;

	virtual const CString &getSwordImage() const = 0;

	virtual const CString &getNickname() const = 0;

	virtual const CString &getChatMsg() const = 0;

	virtual const CString &getImage() const = 0;

	virtual TLevel * getLevel() const = 0;

	virtual int &getAniStep() = 0;

	virtual int getSprite() = 0;

	virtual const CString &getAnimation() const = 0;
};

#endif //GS2EMU_CGANIOBJECTSTUB_H
