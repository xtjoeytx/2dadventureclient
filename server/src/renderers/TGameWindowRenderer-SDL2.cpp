#include "TGameWindow.h"

void TGameWindow::createRenderer() {
	window = SDL_CreateWindow(GSERVER_APPNAME " v" GSERVER_VERSION, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE );

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	renderClear();
	renderPresent();
}

void TGameWindow::renderToggleFullscreen() {
	Uint32 FullscreenFlag = SDL_WINDOW_FULLSCREEN;
	bool IsFullscreen = SDL_GetWindowFlags(window) & FullscreenFlag;
	SDL_SetWindowFullscreen(window, IsFullscreen ? 0 : FullscreenFlag);
	//SDL_ShowCursor(IsFullscreen);
}

void TGameWindow::renderChangeSurfaceSize(SDL_Event * currentEvent) {
	screenWidth = currentEvent->window.data1;
	screenHeight = currentEvent->window.data2;
	renderPresent();
}

void TGameWindow::renderBlit(GameTexture * texture, const SDL_Rect * srcrect, const SDL_Rect * dstrect) {
	SDL_RenderCopy(renderer, texture, srcrect, dstrect);
}

void TGameWindow::renderSetAlpha(GameTexture * texture, Uint8 alpha) {
	SDL_SetTextureAlphaMod(texture, alpha);
}

GameTexture * TGameWindow::renderLoadImage(const char *file) {
	//The final texture
	GameTexture * newTexture = IMG_LoadTexture(renderer, file);

	if( newTexture == nullptr ) {
		client->log( "Unable to create texture from %s! SDL Error: %s\n", file, SDL_GetError() );
		return nullptr;
	}

	return newTexture;
}

GameTexture * TGameWindow::renderText(TTF_Font * font, const char * text, SDL_Color fg) {
	auto * surface = TTF_RenderText_Blended(font, text, fg);

	return SDL_CreateTextureFromSurface(renderer, surface);
}

void TGameWindow::renderDestroyTexture(GameTexture * texture) {
	SDL_DestroyTexture(texture);
}

void TGameWindow::renderQueryTexture(GameTexture * texture, int * w, int * h) {
	SDL_QueryTexture(texture, nullptr, nullptr, w, h);
}

void TGameWindow::renderPresent() { SDL_RenderPresent(renderer); }

void TGameWindow::renderClear() { SDL_RenderClear(renderer); }