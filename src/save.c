#include "emu.h"

int gbmu_save_ext_ram() {
	if (!enable_save_file)
		return 0;
	if (!savefilename)
		return (printf("Error saving game\n"));
	FILE *f;
	if (!(f = fopen(savefilename, "wb")))
		return (printf("Error saving game\n"));
	size_t sz = 32*1024;
	if (fwrite(external_ram, 1, sz, f) != sz) {
		fclose(f);
		return (printf("Error saving game\n"));
	}
	printf("Saved successfully\n");
	fclose(f);
}

int gbmu_load_ext_ram() {
	if (!enable_save_file)
		return 0;
	if (!savefilename)
		return (printf("Error loading save\n"));
	FILE *f;
	if (!(f = fopen(savefilename, "rb")))
		return (printf("Error loading save\n"));
	size_t sz = 32*1024;
	if (fread(external_ram, 1, sz, f) != sz) {
		fclose(f);
		return (printf("Error loading save\n"));
	}
	printf("Loaded save file\n");
	fclose(f);
}