#include "TGameWindow.h"

void TGameWindow::createRenderer() {
	window = SDL_CreateWindow(GSERVER_APPNAME " v" GSERVER_VERSION, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, screenWidth, screenHeight, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
	renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE );

	SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
	SDL_RenderClear(renderer);
	SDL_RenderPresent(renderer);
}

void TGameWindow::renderBlit(GameTexture * texture, const SDL_Rect * srcrect, const SDL_Rect * dstrect) {
	SDL_RenderCopy(renderer, texture, srcrect, dstrect);
}

void TGameWindow::renderSetAlpha(SDL_Texture *texture, Uint8 alpha) {
	SDL_SetTextureAlphaMod(texture, alpha);
}

GameTexture * TGameWindow::renderLoadImage(const char *file) {
	//The final texture
	SDL_Texture* newTexture = IMG_LoadTexture(renderer, file);

	if( newTexture == nullptr ) {
		printf( "Unable to create texture from %s! SDL Error: %s\n", file, SDL_GetError() );
		return nullptr;
	}

	return newTexture;
}

void TGameWindow::renderDestroyTexture(GameTexture * texture) {
	SDL_DestroyTexture(texture);
}

void TGameWindow::renderQueryTexture(GameTexture * texture, int *w, int *h) {
	SDL_QueryTexture(texture, nullptr, nullptr, w, h);
}

void TGameWindow::renderPresent() { SDL_RenderPresent(renderer); }

void TGameWindow::renderClear() { SDL_RenderClear(renderer); }