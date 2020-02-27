#include <functional>
#include <chrono>
#include <atomic>
#include <thread>

#include "main.h"

#include "IDebug.h"
#include "IUtil.h"
#include "IConfig.h"

#include "TMap.h"
#include "TNPC.h"
#include "TWeapon.h"
#include "TPlayer.h"
#include "TClient.h"
#include "TGameWindow.h"
#include "TLevel.h"


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

Uint32 lastTick;

class FocusActionListener : public gcn::FocusListener {
public:
	explicit FocusActionListener(TClient* client);
	// Implement the action function in ActionListener to recieve actions
	// The eventId tells us which widget called the action function.
	void focusGained(const gcn::Event& actionEvent) override {

		if (actionEvent.getSource()->getId() == "ok_clicked") {
			printf("hi");
		}
	}

	void focusLost(const gcn::Event& actionEvent) override {

	}
private:
	TClient* client;
};

FocusActionListener::FocusActionListener(TClient *client) : client(client) {

};

FocusActionListener* focusActionListener;

class ButtonActionListener : public gcn::ActionListener {
public:
	explicit ButtonActionListener(TClient* client);
	// Implement the action function in ActionListener to recieve actions
	// The eventId tells us which widget called the action function.
	void action(const gcn::ActionEvent& actionEvent) override {

		if (actionEvent.getId() == "ok_clicked") {
			if (client->localPlayer)
				client->localPlayer->warp("zold_outside_1_1.nw", client->localPlayer->getX(), client->localPlayer->getY(), -1);
		} else if (actionEvent.getId() == "button2") {
		} else if (actionEvent.getId() == "inputbox_enter") {
			if (client->localPlayer) {
				auto * field = (gcn::TextField*)actionEvent.getSource();
				const char* text = field->getText().c_str();
				client->localPlayer->setProps(CString() >> char(PLPROP_CURCHAT) >> char(strlen(text)) << text, true, false, client->localPlayer);
				field->setText("");
			}
		}
	}
private:
	TClient* client;
};

ButtonActionListener::ButtonActionListener(TClient *client) : client(client) {

};

ButtonActionListener* buttonActionListener;

TGameWindow::TGameWindow(TClient* client) : client(client), tileset(nullptr) {
	screenWidth = SCREEN_WIDTH;
	screenHeight = SCREEN_HEIGHT;
	createRenderer();

	startTimer = std::chrono::high_resolution_clock::now();

	/*
	 * Now it's time for Guichan SDL stuff
	 */
	imageLoader = new gcn::SDLImageLoader();

	// The ImageLoader in use is static and must be set to be
	// able to load images
	gcn::Image::setImageLoader(imageLoader);

	// Set the target for the graphics object to be the screen.
	// In other words, we will draw to the screen.
	// Note, any surface will do, it doesn't have to be the screen.
#if SDL_VERSION_ATLEAST(2,0,0)
	graphics = new gcn::SDL2Graphics();
	graphics->setTarget(renderer, screenWidth, screenHeight);
#else
	graphics = new gcn::SDLGraphics();
	graphics->setTarget(screen);
#endif
	input = new gcn::SDLInput();

	/*
	 * Last but not least it's time to initialize and create the gui
	 * with Guichan stuff.
	 */
	top = new gcn::Container();
	// Set the dimension of the top container to match the screen.
	//top->setDimension(gcn::Rectangle(0, 0, screenWidth*2, screenHeight*2));
	top->setOpaque(false);
	top->setBaseColor(gcn::Color(255, 255, 255, 190));
	top->setSize(screenWidth, screenHeight);

	gui = new gcn::Gui();
	// Set gui to use the SDLGraphics object.
	gui->setGraphics(graphics);
	// Set gui to use the SDLInput object
	gui->setInput(input);
	// Set the top container
	gui->setTop(top);

	// Load the image font.
	font2 = new gcn::ImageFont("fixedfont.bmp", " abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.");
	// The global font is static and must be set.
	gcn::Widget::setGlobalFont(font2);
	//gcn::Widget::setBackgroundColor( gcn::Color( 255, 0, 0, 255 ) );

	// Create a label with test hello world
	label = new gcn::Label("GUI test label");

	// Set the labels position
	label->setPosition(60, 10);
	button = new gcn::Button("OK");
	button->setPosition( 10, 10 );
	button->setActionEventId("ok_clicked");
	// Make an instance of the ButtonActionListener
	buttonActionListener = new ButtonActionListener(client);
	focusActionListener = new FocusActionListener(client);


	// Add the ButtonActionListener to the buttons action listeners
	button->addActionListener(buttonActionListener);

	window2 = new gcn::Window("InGame Window");
	window2->setBaseColor(gcn::Color(192, 192, 192, 255));
	window2->setOpaque(true);
	window2->setMovable(true);
	window2->setTitleBarHeight(20);
	window2->setEnabled(true);
	window2->resizeToContent();
	//window2->setBorderSize(3);

	window2->setSize(200,100);

	// Add the label to the top container
	window2->add(button);
	window2->add(label);

	top->add(window2, 100, 350);

	inputBox = new gcn::TextField("");
	inputBox->setWidth(screenWidth);
	inputBox->setHeight(22);
	//inputBox->setSize(screenWidth, 22);
	//inputBox->setDimension(gcn::Rectangle(0,0,screenWidth,22));
	inputBox->setEnabled(true);

	inputBox->setTabInEnabled(true);
	inputBox->setTabOutEnabled(true);
	inputBox->setBackgroundColor(gcn::Color(255,255,255,255));
	inputBox->setActionEventId("inputbox_enter");
	inputBox->addActionListener(buttonActionListener);
	inputBox->addFocusListener(focusActionListener);
	top->add(inputBox, 0, screenHeight-22);
}

void TGameWindow::init() {
	tileset = TImage::find("zold_tiles_dusty_01.png", client, true);
	if (tileset)
		client->log("Tileset:\t%s\n", tileset->name.text());
	else {
		client->log("\t** [Error] Could not open tileset\n");
		exit(1);
	}

	/* start SDL_ttf */
	if(TTF_Init()==-1)
	{
		client->log("\t** [Error] TTF_Init: %s\n", TTF_GetError());
		exit(1);
	}

	atexit(TTF_Quit); /* remember to quit SDL_ttf */

	font=TTF_OpenFont("8-bit-pusab.ttf", 15);
	fontSmaller=TTF_OpenFont("TEMPSITC.TTF", 20);
	TTF_SetFontStyle(fontSmaller, TTF_STYLE_BOLD);
	//TTF_SetFontOutline(fontSmaller, 1);
	//TTF_SetFontHinting(fontSmaller, TTF_HINTING_NORMAL);


	if(!font)
	{
		client->log("\t** [Error] TTF_OpenFont: %s\n", TTF_GetError());
		exit(3);
	}
	/* end SDL_ttf */

	/* start SDL_mixer */
	/*
	if(Mix_Init(~0)==-1)
	{
		server->log("\t ** [Error]Mix_Init: %s\n", Mix_GetError());
		exit(1);
	}
	atexit(Mix_Quit);

	if(Mix_OpenAudio(MIX_DEFAULT_FREQUENCY, MIX_DEFAULT_FORMAT,2,BUFFER) < 0)
		exit(2);

	auto *music = Mix_LoadMUS("aurora.mod");

	Mix_PlayMusic(music, 0);*/
	/* end SDL_mixer */

	const char* test = "Loading...";

	drawText(font,test, 10, 10, {255,255,255});
}

bool TGameWindow::doMain() {
	sdlEvents();

	drawScreen();

	return true;
}

void TGameWindow::keyPressed(SDL_Keysym *keysym) {
	switch (keysym->sym)
	{
		case SDLK_ESCAPE:
			shutdownProgram = true;
			break;

		case SDLK_f:
			renderToggleFullscreen();
			break;
		case SDLK_q:
			window2->setVisible(!window2->isVisible());
			break;

		default:
			keys[keysym->sym] = true;
			break;
	}
}

void TGameWindow::keyReleased(SDL_Keysym *keysym) {
	switch (keysym->sym)
	{
		default:
			keys[keysym->sym] = false;
			break;
	}
}

void TGameWindow::drawScreen() {
	fps.start();

	Uint32 curTick = SDL_GetTicks();
	auto currentTimer = std::chrono::high_resolution_clock::now();
	typedef std::chrono::duration<float> timeDiff;
	timeDiff time_diff = currentTimer - startTimer;

	// Let the gui perform it's logic (like handle input)
	gui->logic();

	renderClear();

	auto tile = SDL_Rect({ 0, 0, 16, 16});
	auto tileDest = SDL_Rect({2,2,16,16});

	int levelWidth = 64, levelHeight = 64, x, y;
	int cameraWidth = tile.w * levelWidth, cameraHeight = tile.h * levelHeight;

	SDL_Rect chestTile = {static_cast<Sint16>(tile.w * 56),static_cast<Sint16>(tile.h * 15), static_cast<Uint16>(tile.w * 2), static_cast<Uint16>(tile.h * 2)};
	SDL_Rect chestOpenTile = {static_cast<Sint16>(tile.w * 29),static_cast<Sint16>(tile.h * 19), static_cast<Uint16>(tile.w * 2), static_cast<Uint16>(tile.h * 2)};

	TLevel *level;
	int mapWidth = 1, mapHeight = 1;
	CString mapLevels;

	if (client->localPlayer) {
		if ( client->localPlayer->getLevel() != nullptr )
			mapLevels = CString() << client->localPlayer->getLevel()->getLevelName() << "\n";

		TMap *map = client->localPlayer->getLevel()?client->localPlayer->getLevel()->getMap(): nullptr;

		Sint16 cameraX = client->localPlayer->getPixelX()-screenWidth/2, cameraY = client->localPlayer->getPixelY()-screenHeight/2;

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

		std::vector<CAnimationObjectStub *> animationObjects;

		while ( mapLevels.bytesLeft() > 0 ) {
			CString tmpLvlName = mapLevels.readString("\n");
			level = client->getLevel(tmpLvlName.guntokenizeI());

			if ( level ) {
				int gmapX = (level->getMap() ? level->getMap()->getLevelX(level->getLevelName()) : 0);
				int gmapY = (level->getMap() ? level->getMap()->getLevelY(level->getLevelName()) : 0);

				for ( y = 0; y < levelHeight; y++ ) {
					for ( x = 0; x < levelWidth; x++ ) {
						tileDest.x = (x * tile.w) + ((levelWidth * tile.w) * gmapX) - cameraX;
						tileDest.y = (y * tile.h) + ((levelHeight * tile.h) * gmapY) - cameraY;

						if (tileDest.x < -16 || tileDest.x > screenWidth || tileDest.y < -16 || tileDest.y > screenHeight) continue;

						tile.x = TileX(level->getTiles()[y * levelWidth + x], tile.w, tileset->getHeight());
						tile.y = TileY(level->getTiles()[y * levelWidth + x], tile.w, tileset->getHeight());

						tileset->render(tileDest.x, tileDest.y, tile.x, tile.y, tile.w, tile.h);
					}
				}

				/*
				for (auto *sign : *level->getLevelSigns()) {
					auto signDest = SDL_Rect({static_cast<Sint16>((sign->getX()*tile.w) + ((levelWidth * tile.w) * gmapX) - cameraX),static_cast<Sint16>((sign->getY() * tile.h) + ((levelHeight * tile.h) * gmapY) - cameraY),32,16});
					SDL_SetRenderDrawColor(renderer, 255,0,0,255);

					SDL_RenderDrawRect(renderer, &signDest);
				}

				for (auto *link : *level->getLevelLinks()) {
					auto linkDest = SDL_Rect({static_cast<Sint16>((link->getX()*tile.w) + ((levelWidth * tile.w) * gmapX) - cameraX),static_cast<Sint16>((link->getY() * tile.h) + ((levelHeight * tile.h) * gmapY) - cameraY),link->getWidth(),link->getHeight()});
					SDL_SetRenderDrawColor(renderer, 0,255,255,255);

					SDL_RenderDrawRect(renderer, &linkDest);
				}

				SDL_SetRenderDrawColor(renderer, 0,0,0,0);
				*/

				for (auto *chest : *level->getLevelChests()) {
					auto chestDest = SDL_Rect({ static_cast<Sint16>((chest->getX() * tile.w) + ((levelWidth * tile.w) * gmapX) - cameraX), static_cast<Sint16>((chest->getY() * tile.h) + ((levelHeight * tile.h) * gmapY) - cameraY), 32, 32});

					if (client->localPlayer->hasChest(chest,level->getLevelName()))
						tileset->render(chestDest.x, chestDest.y, chestOpenTile.x, chestOpenTile.y, chestOpenTile.w, chestOpenTile.h);
					else
						tileset->render(chestDest.x, chestDest.y, chestTile.x, chestTile.y, chestTile.w, chestTile.h);
				}

				for (auto *levelPlayer : *level->getPlayerList()) {
					if (levelPlayer != nullptr)
						animationObjects.push_back(levelPlayer);

				}

				for (auto *levelNpc : *level->getLevelNPCs()) {
					if (levelNpc != nullptr) {
						animationObjects.push_back(levelNpc);
					}
				}
			}
		}

		std::sort(animationObjects.begin(), animationObjects.end(), [ ]( CAnimationObjectStub* lhs, CAnimationObjectStub* rhs )
		{
			return lhs->getPixelY() < rhs->getPixelY();
		});

		for (auto animationObject : animationObjects) {
			if (!animationObject->getImage().match("#c#")) {
				TImage* tmpImg = TImage::find(animationObject->getImage().text(), client);
				if (tmpImg)
					tmpImg->render(((levelWidth * tile.w) * (animationObject->getLevel()->getMap() ? animationObject->getLevel()->getMap()->getLevelX(animationObject->getLevel()->getLevelName()) : 0)) + animationObject->getPixelX() - cameraX, ((levelHeight * tile.h) * (animationObject->getLevel()->getMap() ? animationObject->getLevel()->getMap()->getLevelY(animationObject->getLevel()->getLevelName()) : 0)) + animationObject->getPixelY()- cameraY, 0, 0, animationObject->getWidth(), animationObject->getHeight());
			} else if (animationObject->getImage().match("#c#")) {
				drawAnimation(animationObject, ((levelWidth * tile.w) * (animationObject->getLevel()->getMap() ? animationObject->getLevel()->getMap()->getLevelX(animationObject->getLevel()->getLevelName()) : 0)) + animationObject->getPixelX() - cameraX, ((levelHeight * tile.h) * (animationObject->getLevel()->getMap() ? animationObject->getLevel()->getMap()->getLevelY(animationObject->getLevel()->getLevelName()) : 0)) + animationObject->getPixelY() - cameraY, time_diff.count());
			}
		}

		animationObjects.clear();
	}

	//Increment the frame counter
	frame++;

	if (lastTick < curTick - 1000) {
		lastTick = curTick;
		fps_current = frame;
		frame = 0;
	}

	char fps_text[50];
	sprintf(fps_text, "FPS: %d", fps_current);

	drawText(font, fps_text, 10, 10, { static_cast<Uint8>(fps_current), static_cast<Uint8>(fps_current), static_cast<Uint8>(fps_current) });

	// Draw the gui
	gui->draw();

	renderPresent();

	if ( fps.get_ticks() < 1000 / FRAMES_PER_SECOND ) {
		SDL_Delay((1000 / FRAMES_PER_SECOND) - fps.get_ticks());
	}

	startTimer = currentTimer;
}

void TGameWindow::drawText(TTF_Font * textFont, const char * text, int x, int y, SDL_Color color, TextPosition p) {
	//sdlEvents();
	auto * surf = renderText(textFont, text, color);
	int w, h;

	renderQueryTexture(surf, &w, &h);

	if(surf) {
		switch (p){
			case RIGHT:
				x = x - w;
				break;
			case CENTERED:
				x = x - (int)(w/2);
				break;
			case LEFT:
			default:
				break;
		}
		auto dest = SDL_Rect{ x, y, static_cast<Uint16>(w), static_cast<Uint16>(h) };

		renderBlit(surf, nullptr, &dest);
		renderDestroyTexture(surf);
		//prevY += h-5;
	}
}

void TGameWindow::sdlEvents() {
	while ( SDL_PollEvent(&event) ) {
		if ( event.type == SDL_QUIT ) {
			shutdownProgram = true;
		}

		if (RENDER_RESIZE_EVENT) {
			renderChangeSurfaceSize();
			top->setSize(screenWidth, screenHeight);
			inputBox->setPosition(0, screenHeight-22);
			inputBox->setWidth(screenWidth);
		}

		if ( event.type == SDL_KEYDOWN ) {
			keyPressed(&event.key.keysym);
		}

		if ( event.type == SDL_KEYUP ) {
			keyReleased(&event.key.keysym);
		}

		input->pushInput(event);
	}

	float speed = 0.30f;
	int moveX = 0, moveY = 0, dir = 0;
	if (client->localPlayer)
		dir = client->localPlayer->getSprite() % 4;

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

	int testx = ((client->localPlayer->getPixelX()+moveX+32)/16);
	int testy = ((client->localPlayer->getPixelY()+moveY+44)/16);

	if (!client->localPlayer->getLevel()->isOnWall(testx,testy)) {
		int x2 = client->localPlayer->getPixelX() + moveX;
		int y2 = client->localPlayer->getPixelY() + moveY;

		unsigned short fixedX = abs(x2) << 1, fixedY = abs(y2) << 1;
		if (x2 < 0) fixedX |= 0x0001;
		if (y2 < 0) fixedY |= 0x0001;

		client->localPlayer->setProps(CString() >> char(PLPROP_X2) >> short(fixedX) >> char(PLPROP_Y2) >> short(fixedY), true, false, client->localPlayer);
		client->localPlayer->setProps(CString() >> char(PLPROP_SPRITE) >> char(dir) >> char(PLPROP_GANI) >> char(strlen(ani)) << ani, true, false, client->localPlayer);
	}
}

void TGameWindow::drawAnimation(CAnimationObjectStub* animationObject, int x, int y, float time) {
	auto *ani = TAnimation::find(CString(CString() << animationObject->getAnimation().text() << ".gani").text(), client);

	if (ani != nullptr)
		ani->render(animationObject, x, y, animationObject->getSprite(), &animationObject->getAniStep(), time);
}

TGameWindow::~TGameWindow() {
	fps.stop();
}
