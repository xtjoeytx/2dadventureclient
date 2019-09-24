
#include "TGameWindow.h"
#include "main.h"

#include "TMap.h"
#include "TNPC.h"
#include "IUtil.h"
#include "TWeapon.h"
#include "TPlayer.h"

#include "TServer.h"
#include "TLevel.h"
#include <SDL_image.h>
#include <SDL.h>
#include <functional>
#include <chrono>
#include <atomic>
#include <thread>
#include "IDebug.h"

#ifdef V8NPCSERVER
#include "CScriptEngine.h"
#endif


#define TileIndex(x, y, tileWidth, tilesetHeight) (x / tileWidth * tilesetHeight + x % tileWidth + y * tileWidth)
#define TileX(levelTile, tileWidth, tilesetHeight)  (( ( levelTile / tilesetHeight ) * tileWidth + levelTile % tileWidth ) * tileWidth)
#define TileY(levelTile, tileWidth, tilesetHeight)  (( ( levelTile % tilesetHeight  ) / tileWidth ) * tileWidth)
static const Uint32 rmask = 0x000000ff;
static const Uint32 gmask = 0x0000ff00;
static const Uint32 bmask = 0x00ff0000;
static const Uint32 amask = 0xff000000;
static Sint16 screenWidth = 800;
static Sint16 screenHeight = 600;

Uint32 lastTick;

TGameWindow::TGameWindow(TServer* server) : server(server), pics1(nullptr) {
	window = SDL_CreateWindow("GS2Emu", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE );

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);

	startTimer = std::chrono::high_resolution_clock::now();
}

void TGameWindow::init() {
	pics1 = TImage::find("pics1.png", server);
}

bool TGameWindow::doMain() {
	SDLEvents();

	DrawScreen();

	return true;
}

void TGameWindow::ToggleFullscreen(SDL_Window* window) {
	Uint32 FullscreenFlag = SDL_WINDOW_FULLSCREEN;
	bool IsFullscreen = SDL_GetWindowFlags(window) & FullscreenFlag;
	SDL_SetWindowFullscreen(window, IsFullscreen ? 0 : FullscreenFlag);
	SDL_ShowCursor(IsFullscreen);
}

void TGameWindow::keyPressed(SDL_Keysym *keysym)
{
	switch (keysym->sym)
	{
		case SDLK_ESCAPE:
			shutdownProgram = true;
			break;

		case SDLK_f:
			TGameWindow::ToggleFullscreen(window );
			break;

		default:
			keys[keysym->sym] = true;
			break;
	}
}

void TGameWindow::keyReleased(SDL_Keysym *keysym)
{
	switch (keysym->sym)
	{
		default:
			keys[keysym->sym] = false;
			break;
	}
}

void TGameWindow::DrawScreen() {
	Uint32 curTick = SDL_GetTicks();
	auto currentTimer = std::chrono::high_resolution_clock::now();
	typedef std::chrono::duration<float> timeDiff;
	timeDiff time_diff = currentTimer - startTimer;
	if (curTick - lastTick >= (1000 / FRAMES_PER_SECOND)) { // 60 fps
		SDL_RenderClear(renderer);

		auto tile = SDL_Rect({ 0, 0, 16, 16});
		auto tileDest = SDL_Rect({2,2,16,16});

		int levelWidth = 64, levelHeight = 64, x, y;
		int cameraWidth = tile.w * levelWidth, cameraHeight = tile.h * levelHeight;

		SDL_Rect chestTile = {static_cast<Sint16>(tile.w * 56),static_cast<Sint16>(tile.h * 15), static_cast<Uint16>(tile.w * 2), static_cast<Uint16>(tile.h * 2)};
		SDL_Rect chestOpenTile = {static_cast<Sint16>(tile.w * 29),static_cast<Sint16>(tile.h * 19), static_cast<Uint16>(tile.w * 2), static_cast<Uint16>(tile.h * 2)};

		TLevel *level;
		int mapWidth = 1, mapHeight = 1;
		CString mapLevels;

		if (server->localPlayer) {
			if ( server->localPlayer->getLevel() != nullptr )
				mapLevels = CString() << server->localPlayer->getLevel()->getLevelName() << "\n";

			TMap *map = server->localPlayer->getLevel()?server->localPlayer->getLevel()->getMap(): nullptr;

			Sint16 cameraX = server->localPlayer->getPixelX()-screenWidth/2, cameraY = server->localPlayer->getPixelY()-screenHeight/2;

			if (map) {
				mapWidth = map->getWidth();
				mapHeight = map->getHeight();
			}

			int cameraMaxX = (cameraWidth * mapWidth) - screenWidth, cameraMaxY = (cameraHeight * mapHeight) - screenHeight;

			if (cameraX > cameraMaxX) cameraX = cameraMaxX;
			if (cameraY > cameraMaxY) cameraY = cameraMaxY;

			if (cameraX < 0) cameraX = 0;
			if (cameraY < 0) cameraY = 0;

			if (map)
				mapLevels = map->getLevels();

			std::vector<CGaniObjectStub *> ganiObjects;

			while ( mapLevels.bytesLeft() > 0 ) {
				CString tmpLvlName = mapLevels.readString("\n");
				level = server->getLevel(tmpLvlName.guntokenizeI());

				int gmapX = (level->getMap() ? level->getMap()->getLevelX(level->getLevelName()) : 0);
				int gmapY = (level->getMap() ? level->getMap()->getLevelY(level->getLevelName()) : 0);

				if ( level ) {
					for ( y = 0; y < levelHeight; y++ ) {
						for ( x = 0; x < levelWidth; x++ ) {
							tileDest.x = (x * tile.w) + ((levelWidth * tile.w) * gmapX) - cameraX;
							tileDest.y = (y * tile.h) + ((levelHeight * tile.h) * gmapY) - cameraY;

							if (tileDest.x < -16 || tileDest.x > screenWidth || tileDest.y < -16 || tileDest.y > screenHeight) continue;

							tile.x = TileX(level->getTiles()[y * levelWidth + x], tile.w, pics1->getHeight());
							tile.y = TileY(level->getTiles()[y * levelWidth + x], tile.w, pics1->getHeight());

							pics1->render(tileDest.x, tileDest.y, tile.x, tile.y, tile.w, tile.h, SDL_ALPHA_OPAQUE);
						}
					}

					/*
					 * We don't need to render signs...
					 * for (auto *sign : *level->getLevelSigns()) {
					 *	auto signDest = SDL_Rect({static_cast<Sint16>((sign->getX()*tile.w) + ((levelWidth * tile.w) * gmapX) - cameraX),static_cast<Sint16>((sign->getY() * tile.h) + ((levelHeight * tile.h) * gmapY) - cameraY),32,16});
					 * }
					*/

					for (auto *chest : *level->getLevelChests()) {
						auto chestDest = SDL_Rect({ static_cast<Sint16>((chest->getX() * tile.w) + ((levelWidth * tile.w) * gmapX) - cameraX), static_cast<Sint16>((chest->getY() * tile.h) + ((levelHeight * tile.h) * gmapY) - cameraY), 32, 32});

						if (server->localPlayer->hasChest(chest,level->getLevelName()))
							pics1->render(chestDest.x, chestDest.y, chestOpenTile.x, chestOpenTile.y, chestOpenTile.w, chestOpenTile.h, SDL_ALPHA_OPAQUE);
						else
							pics1->render(chestDest.x, chestDest.y, chestTile.x, chestTile.y, chestTile.w, chestTile.h, SDL_ALPHA_OPAQUE);
					}

					for (auto *levelPlayer : *level->getPlayerList()) {
						ganiObjects.push_back(levelPlayer);
					}

					for (auto *levelNpc : *level->getLevelNPCs()) {
						ganiObjects.push_back(levelNpc);
					}
				}
			}

			for (auto &ganiObj : ganiObjects) {
				int dir = ganiObj->getSprite();

				GaniDraw(ganiObj, ganiObj->getAnimation(), ((levelWidth * tile.w) * (ganiObj->getLevel()->getMap() ? ganiObj->getLevel()->getMap()->getLevelX(ganiObj->getLevel()->getLevelName()) : 0)) + ganiObj->getPixelX() - cameraX, ((levelHeight * tile.h) * (ganiObj->getLevel()->getMap() ? ganiObj->getLevel()->getMap()->getLevelY(ganiObj->getLevel()->getLevelName()) : 0)) + ganiObj->getPixelY() - cameraY, dir, time_diff.count());
			}

			ganiObjects.clear();
		}

		SDL_RenderPresent(renderer);

/*
		if ( fps.get_ticks() < 1000 / FRAMES_PER_SECOND ) {
			SDL_Delay((1000 / FRAMES_PER_SECOND) - fps.get_ticks());
		}
*/

		lastTick = curTick;
		startTimer = currentTimer;
	}
	else SDL_Delay(1);
}

void TGameWindow::SDLEvents() {

	//const Uint8 keys = SDL_GetKeyboardState(nullptr);
	if ( SDL_PollEvent(&event)) {
		if ( event.type == SDL_QUIT ) {
			shutdownProgram = true;
		}

		if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
			screenWidth = event.window.data1;
			screenHeight = event.window.data2;
			SDL_RenderPresent( renderer );
		}
/*
		if ( event.type == SDL_WINDOWEVENT_RESIZED) {
			//SDL_FreeSurface(screen);
			screenWidth = event.window.event;
			screenHeight = event.resize.h;
			//ChangeSurfaceSize();
		}
*/
		if ( event.type == SDL_KEYDOWN ) {
			keyPressed(&event.key.keysym);
		}

		if ( event.type == SDL_KEYUP ) {
			keyReleased(&event.key.keysym);
		}
	}

	float speed = 0.2;
	int moveX = 0, moveY = 0, dir = server->localPlayer->getSprite() % 4;

	auto ani = "idle";

	if (keys[SDLK_UP])
	{
		moveY -= 16.0f*speed;
		dir = 0;
		ani = "walk";
	}

	if (keys[SDLK_LEFT])
	{
		moveX -= 16.0*speed;
		dir = 1;
		ani = "walk";
	}

	if (keys[SDLK_DOWN])
	{
		moveY += 16.0f*speed;
		dir = 2;
		ani = "walk";
	}

	if (keys[SDLK_RIGHT])
	{
		moveX += 16.0f*speed;
		dir = 3;
		ani = "walk";
	}

	int x2 = server->localPlayer->getPixelX() + moveX;
	int y2 = server->localPlayer->getPixelY() + moveY;

	unsigned short fixedX = abs(x2) << 1, fixedY = abs(y2) << 1;
	if (x2 < 0) fixedX |= 0x0001;
	if (y2 < 0) fixedY |= 0x0001;

	server->localPlayer->setProps(CString() >> char(PLPROP_X2) >> short(fixedX) >> char(PLPROP_Y2) >> short(fixedY), true, false, server->localPlayer);
	server->localPlayer->setProps(CString() >> char(PLPROP_SPRITE) >> char(dir) >> char(PLPROP_GANI) >> char(strlen(ani)) << ani, true, false, server->localPlayer);
}

void TGameWindow::GaniDraw(CGaniObjectStub* player, const CString &animation, int x, int y, int dir, float time) {
	auto *ani = TAnimation::find(CString(CString() << animation.text() << ".gani").text(), server);

	if (ani != nullptr)
		ani->render(player, server, x, y, dir, &player->getAniStep(), time);
}

TGameWindow::~TGameWindow() {

}
