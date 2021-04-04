#ifdef GBMU_USE_SDL

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#include "emu.h"

#define WINDOW_TITLE "Keys: W,A,S,D,J,K,Enter,Shift"

SDL_Window *window;
SDL_Renderer *renderer;
SDL_Texture *screenTexture;
SDL_mutex *screenTextureMutex;
bool isPaused;

void quit() {
	SDL_CloseAudio();
	SDL_DestroyMutex(screenTextureMutex);
	gbmu_quit();
	exit(EXIT_SUCCESS);
}

void frame_is_ready() {
	if (SDL_LockMutex(screenTextureMutex) == 0) {
		SDL_UpdateTexture(screenTexture, NULL, screen_pixels, 160*sizeof(uint32_t));
		SDL_UnlockMutex(screenTextureMutex);
	} else {
		fprintf(stderr, "Couldn't lock mutex\n");
	}
}

void myAudioCallback(void* userdata, Uint8* stream, int len) {
	if (isPaused) {
		memset(stream, 0, len);
		return;
	}
	gbmu_fill_audio_buffer(stream, len, &frame_is_ready);
}

void start_audio() {
	SDL_AudioSpec want, have;

	SDL_memset(&want, 0, sizeof(want));
	want.freq = 44100;
	want.format = AUDIO_F32SYS;
	want.channels = 1;
	want.samples = 4096;
	want.callback = myAudioCallback;

	if (SDL_OpenAudio(&want, &have) < 0)
		SDL_Log("Failed to open audio: %s", SDL_GetError());
	if (have.format != want.format)
		SDL_Log("We didn't get Float32 audio format.");
	// printf("want.samples: %u\n", want.samples);
	// printf("have.samples: %u\n", have.samples);
	// printf("want.freq: %d\n", want.freq);
	// printf("have.freq: %d\n", have.freq);
	// printf("have.channels: %d\n", have.channels);
	num_audio_channels = have.channels;
	SDL_PauseAudio(0); /* start audio playing. */
}

int main(int ac, char ** av) {

	if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
		SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
		return 1;
	}
	SDL_CreateWindowAndRenderer(160*2, 144*2, SDL_WINDOW_RESIZABLE, &window, &renderer);
	SDL_SetWindowTitle(window, WINDOW_TITLE);
	SDL_RenderSetLogicalSize(renderer, 160, 144); // Keep a fixed aspect ratio
	// SDL_RenderSetIntegerScale(renderer, true); // Force integer scaling
	screenTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, 160, 144);
	screenTextureMutex = SDL_CreateMutex();

	gbmu_init();
	gbmu_load_rom(av[1]);

	start_audio();

	while (true) {
		SDL_Event e;
		while (SDL_PollEvent(&e)) {
			if (e.type == SDL_QUIT)
				quit();
			if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.scancode == SDL_SCANCODE_ESCAPE)
					quit();
				if (e.key.keysym.scancode == SDL_SCANCODE_PAUSE) {
					isPaused = !isPaused;
					SDL_SetWindowTitle(window, isPaused ? "[Paused] "WINDOW_TITLE : WINDOW_TITLE);
				}
				if (e.key.keysym.scancode == SDL_SCANCODE_KP_MULTIPLY)
					gbmu_reset();
			}
		}
		const uint8_t *sdl_keys = SDL_GetKeyboardState(NULL);
		gbmu_keys[GBMU_SELECT] = sdl_keys[SDL_SCANCODE_LSHIFT] || sdl_keys[SDL_SCANCODE_RSHIFT];
		gbmu_keys[GBMU_START]  = sdl_keys[SDL_SCANCODE_RETURN];
		gbmu_keys[GBMU_B]      = sdl_keys[SDL_SCANCODE_J];
		gbmu_keys[GBMU_A]      = sdl_keys[SDL_SCANCODE_K];
		gbmu_keys[GBMU_UP]     = sdl_keys[SDL_SCANCODE_W];
		gbmu_keys[GBMU_LEFT]   = sdl_keys[SDL_SCANCODE_A];
		gbmu_keys[GBMU_DOWN]   = sdl_keys[SDL_SCANCODE_S];
		gbmu_keys[GBMU_RIGHT]  = sdl_keys[SDL_SCANCODE_D];

		if (SDL_LockMutex(screenTextureMutex) == 0) {
			SDL_RenderCopy(renderer, screenTexture, NULL, NULL);
			SDL_RenderPresent(renderer);
			SDL_UnlockMutex(screenTextureMutex);
		} else {
			fprintf(stderr, "Couldn't lock mutex\n");
		}
		SDL_Delay(16);
	}

}

#endif
