#ifndef GS2EMU_CGANIOBJECTSTUB_H
#define GS2EMU_CGANIOBJECTSTUB_H

#include <CString.h>

class CGaniObjectStub {
public:
	virtual float getX() const = 0;

	virtual float getY() const = 0;

	virtual int getPixelX() const = 0;

	virtual int getPixelY() const = 0;

//	virtual int getHeight() const = 0;

//	virtual int getWidth() const = 0;

	virtual const CString &getBodyImage() const = 0;

	virtual const CString &getHeadImage() const = 0;

//	virtual const CString &getHorseImage() const = 0;

	virtual const CString &getShieldImage() const = 0;

	virtual const CString &getSwordImage() const = 0;

	virtual const CString &getNickname() const = 0;

	virtual int &getAniStep() = 0;
};
#endif //GS2EMU_CGANIOBJECTSTUB_H
