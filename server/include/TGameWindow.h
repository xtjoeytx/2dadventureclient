#ifndef GS2EMU_TGAMEWINDOW_H
#define GS2EMU_TGAMEWINDOW_H

#include <vector>
#include <map>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#include "IConfig.h"

#include "TClient.h"

#if SDL_VERSION_ATLEAST(2,0,0)
#define GameTexture SDL_Texture
#define SDL_VIDEORESIZE SDL_LASTEVENT+1
#else
#define GameTexture SDL_Surface
#endif

static std::map<Sint32, bool> keys;
static TTF_Font *font;
static int BUFFER;
static Sint16 screenWidth = 640;
static Sint16 screenHeight = 480;

class TImage;

class TGameWindow
{
public:
	explicit TGameWindow(TClient* server);

	~TGameWindow();

	void init();

	bool doMain();

	TImage *tileset{};

	void keyPressed(SDL_Keysym *keysym);

	static void keyReleased(SDL_Keysym *keysym);

	void drawText(const char *text);

	void renderClear();

	void renderPresent();

	void renderBlit(GameTexture * texture, const SDL_Rect * srcrect, const SDL_Rect * dstrect);

	static void renderQueryTexture(GameTexture * texture, int *w, int *h);

	GameTexture * renderLoadImage(const char *file);

	GameTexture * renderText(TTF_Font *font, const char *text, SDL_Color fg);

	static void renderSetAlpha(SDL_Texture * texture, Uint8 alpha);

	static void renderDestroyTexture(GameTexture * texture);

	void renderToggleFullscreen();

	void renderChangeSurfaceSize(SDL_Event * event);
private:
	TClient * client;
	std::chrono::high_resolution_clock::time_point startTimer;

	void SDLEvents();

	void GaniDraw(CGaniObjectStub * player, int x, int y, float time);

	void DrawScreen();
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_Window *window{};
	SDL_Renderer *renderer{};
#else
	SDL_Surface *screen{};
#endif

	void createRenderer();

};

#endif //GS2EMU_TGAMEWINDOW_H
