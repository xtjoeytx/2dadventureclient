#ifndef GS2EMU_TGAMEWINDOW_H
#define GS2EMU_TGAMEWINDOW_H

#include <vector>
#include <map>

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#include <SDL_mixer.h>

#if SDL_VERSION_ATLEAST(2,0,0)
#include <guisan.hpp>
#include <guisan/sdl.hpp>
#else
#include <guichan.hpp>
#include <guichan/sdl.hpp>
#endif
#include "IConfig.h"

#include "TClient.h"

#if SDL_VERSION_ATLEAST(2,0,0)
#define GameTexture SDL_Texture
#define RENDER_RESIZE_EVENT event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED
#else
#define GameTexture SDL_Surface
#define SDL_Keysym SDL_keysym
#define RENDER_RESIZE_EVENT event.type == SDL_VIDEORESIZE
#endif

#define SCREEN_WIDTH 1280
#define SCREEN_HEIGHT 720

enum TextPosition {
	LEFT,
	RIGHT,
	CENTERED
};

class TImage;

class TGameWindow
{
public:
	explicit TGameWindow(TClient* client);

	~TGameWindow();

	void init();

	bool doMain();

	void drawText(TTF_Font * textFont, const char * text, int x, int y, SDL_Color color, TextPosition p = LEFT);

	static void renderSetAlpha(GameTexture * texture, Uint8 alpha);

	static void renderDestroyTexture(GameTexture * texture);

	static void renderQueryTexture(GameTexture * texture, int *w, int *h);

	GameTexture * renderLoadImage(const char *file);

	void renderBlit(GameTexture * texture, SDL_Rect * srcrect, SDL_Rect * dstrect);

	void renderClear();

	void renderPresent();

	void renderSetWindowSize(int w, int h);

	TTF_Font *font;
	TTF_Font *fontSmaller;
private:

	TClient * client;
	TImage *tileset{};

	void keyPressed(SDL_Keysym * keysym);

	void keyReleased(SDL_Keysym * keysym);

	std::chrono::high_resolution_clock::time_point startTimer;
	Sint16 screenWidth;
	Sint16 screenHeight;
	const static int FRAMES_PER_SECOND = 60;
	SDL_Event event;
	int frame = 0;
	Timer fps;

	int prevY;
	std::map<Sint32, bool> keys;
	static int BUFFER;

	void sdlEvents();

	void drawAnimation(CAnimationObjectStub * animationObject, int x, int y, float time);

	void drawScreen();

	/* Renderer stuff */
#if SDL_VERSION_ATLEAST(2,0,0)
	SDL_Window *window{};

	SDL_Renderer *renderer{};
#else
	SDL_Surface *screen{};
#endif

	void createRenderer();

	GameTexture * renderText(TTF_Font *font, const char *text, SDL_Color fg);

	void renderToggleFullscreen();

	void renderChangeSurfaceSize();

	int fps_current;

	/* Guichan SDL stuff we need */
	gcn::SDLInput* input;             // Input driver
#if SDL_VERSION_ATLEAST(2,0,0)
	gcn::SDL2Graphics* graphics;       // Graphics driver
#else
	gcn::SDLGraphics* graphics;       // Graphics driver
#endif
	gcn::SDLImageLoader* imageLoader; // For loading images

	/* Guichan stuff we need */
	gcn::Gui* gui;            // A Gui object - binds it all together
	gcn::Container* top;      // A top container
	gcn::ImageFont* font2;     // A font
	gcn::Label* label;        // And a label for the Hello World text
	gcn::Button* button;
	gcn::Window* window2;
	gcn::TextField* inputBox;
};

#endif //GS2EMU_TGAMEWINDOW_H
