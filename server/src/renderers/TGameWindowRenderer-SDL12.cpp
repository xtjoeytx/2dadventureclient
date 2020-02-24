#include "TGameWindow.h"

void TGameWindow::createRenderer() {
	screen = SDL_SetVideoMode(screenWidth, screenHeight, SDL_SWSURFACE | SDL_RESIZABLE);

	SDL_WM_SetCaption(GSERVER_APPNAME " v" GSERVER_VERSION, nullptr);

	renderClear();
	renderPresent( );
}

void TGameWindow::renderToggleFullscreen() {
	SDL_WM_ToggleFullscreen(screen);
}

void TGameWindow::renderChangeSurfaceSize(SDL_Event * event) {
	SDL_FreeSurface(screen);
	screenWidth = event->resize.w;
	screenHeight = event->resize.h;
	createRenderer();
}

void TGameWindow::renderBlit(GameTexture * texture, const SDL_Rect * srcrect, const SDL_Rect * dstrect) {
	SDL_BlitSurface(texture, srcrect, screen, dstrect);
}

void TGameWindow::renderSetAlpha(GameTexture *texture, Uint8 alpha) {
	SDL_SetAlpha(texture, SDL_SRCALPHA, alpha);
}

GameTexture * TGameWindow::renderLoadImage(const char *file) {
	//The final texture
	GameTexture* newTexture = IMG_Load(file);

	if( newTexture == nullptr ) {
		client->log( "Unable to create texture from %s! SDL Error: %s\n", file, SDL_ImageError() );
		return nullptr;
	}

	return newTexture;
}

GameTexture * TGameWindow::renderText(TTF_Font *font, const char *text, SDL_Color fg) {
	return TTF_RenderText_Blended(font, text, fg);
}

void TGameWindow::renderDestroyTexture(GameTexture * texture) {
	SDL_FreeSurface(texture);
}

void TGameWindow::renderQueryTexture(GameTexture * texture, int *w, int *h) {
	w = texture->w;
	h = texture->h;
}

void TGameWindow::renderPresent() { SDL_Flip(screen); }

void TGameWindow::renderClear() { SDL_FillRect(screen, nullptr, 0); }