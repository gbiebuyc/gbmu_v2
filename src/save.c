#include "emu.h"
#include <errno.h>
#include <fcntl.h>

#define SAVE_SIZE 32*1024

void gbmu_save_ext_ram() {
	if (getenv("GBMU_SKIP_SAVE") || !cartridgeHasBattery)
		return;
	int fd;
	if ((fd = open(savefilename, O_WRONLY|O_CREAT|O_BINARY, 0666)) < 0) {
		perror("Error saving game: open");
		return;
	}
	ssize_t ret = write(fd, external_ram, SAVE_SIZE);
	if (ret < 0)
		perror("Error saving game: write");
	else if (ret < SAVE_SIZE)
		printf("Error saving game: write: end-of-file\n");
	else
		printf("Saved successfully\n");
	close(fd);
}

void gbmu_load_ext_ram() {
	if (getenv("GBMU_SKIP_SAVE") || !cartridgeHasBattery)
		return;
	int fd;
	if ((fd = open(savefilename, O_RDONLY|O_BINARY)) < 0) {
		if (errno == ENOENT)
			return; // Not yet created. Ignore.
		perror("Error loading save: open");
		return;
	}
	ssize_t ret = read(fd, external_ram, SAVE_SIZE);
	if (ret < 0)
		perror("Error loading save: read");
	else if (ret < SAVE_SIZE)
		printf("Error loading save: read: end-of-file\n");
	else
		printf("Loaded save file\n");
	close(fd);
}
