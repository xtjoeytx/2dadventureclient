#ifndef GS2EMU_TGAMEWINDOW_H
#define GS2EMU_TGAMEWINDOW_H

#include <SDL.h>
#include <vector>
#include <map>
#include "TServer.h"

static std::map<Sint32, bool> keys;
class TImage;

class TGameWindow
{
public:
	TGameWindow(TServer* server);
	~TGameWindow();
	void init();
	bool doMain();
	void ToggleFullscreen(SDL_Window* window);
	SDL_Window *window;
	TImage *pics1;
	SDL_Renderer *renderer;
	void keyPressed(SDL_Keysym *keysym);
	void keyReleased(SDL_Keysym *keysym);

private:
	TServer * server;
	std::chrono::high_resolution_clock::time_point startTimer;

	void SDLEvents();

	void GaniDraw(CGaniObjectStub * player, const CString &animation, int x, int y, int dir, float time);

	void DrawScreen();
};

#endif //GS2EMU_TGAMEWINDOW_H
