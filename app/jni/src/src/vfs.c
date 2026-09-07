//#define DEBUG_VFS
#include <stdint.h>
#include <SDL/SDL.h>
#include "vfs.h"

#define COMP_BUFFER_DYN

PAK paks[0x10];
int paks_count;
FSLI files[0x2000];
int files_count;

#ifdef COMP_BUFFER_DYN
	/* Use static buffers to avoid malloc/free on every image load */
	static uint8_t temp1_buf[0x100000];  /* 1MB for compressed data */
	static uint8_t temp2_buf[0x100000];  /* 1MB for decompressed data */
	uint8_t *temp1 = temp1_buf;
	uint8_t *temp2 = temp2_buf;
	
	void temp_update(uint8_t **ptr, int newlen) {
		/* No-op: buffers are static now */
	}
#else
	uint8_t temp1[0xD0000];
	uint8_t temp2[0xE0000];

	void temp_update(uint8_t **ptr, int newlen) {
	}
#endif

int rw_file_exists(char *name) {
	int exists = 0;
	SDL_RWops *f = SDL_RWFromFile(name, "rb");
	if (f)
	{
		exists = 1;
		SDL_RWclose(f);
	}
	return exists;
}

int file_exists(char *name) {
	int exists = 0;
	SDL_RWops *f;
	f = SDL_RWFromFile(name, "rb");
	if (f)
	{
		exists = 1;
		SDL_RWclose(f);
	}
	return exists;
}

int debug_file_exists(char *name) {
	int exists = 0;
	SDL_RWops *f;
	printf("debug_file_exists('%s') 1\n", name);
	f = SDL_RWFromFile(name, "rb");
	printf("debug_file_exists('%s') 2\n", name);
	if (f)
	{
		exists = 1;
		SDL_RWclose(f);
	}
	printf("debug_file_exists('%s') 3\n", name);
	return exists;
}

void VFS_RESET() {
	//int n; for (n = 0; n < paks_count; n++) SDL_RWclose(paks[n].rw);
	files_count = 0;
	paks_count = 0;
}

int VFS_MOUNT(char *name) {
	SDL_RWops *f;
	PAK *pak;
	printf("Mounting '%s'...", name);
	if ((f = SDL_RWFromFile(name, "rb")) == NULL) {
		printf("Error\n");
		return 0;
	}
	
	pak = &paks[paks_count++];
	
	strcpy(pak->path, name);
	pak->rw = f;
	
	uint32_t n, pos, len, offset;
	uint16_t count;
	SDL_RWseek(f, 8, SEEK_SET);
	SDL_RWread(f, &count , sizeof(count), 1);
	SDL_RWread(f, &offset, sizeof(offset), 1);
	SDL_RWseek(f, offset, SEEK_SET);
	pos = 0x10;
	for (n = 0; n < count; n++) {
		FSLI *slice = &files[files_count++];

		SDL_RWread(f, slice->name, 1, 0xC);
		SDL_RWread(f, &len, sizeof(len), 1);

		slice->name[0xC] = 0;

		slice->pak = pak;
		slice->pos = pos;
		slice->len = len;
		
		pos += len;
	}
	printf("OK (%d)\n", count);
	
	SDL_RWclose(f);
	
	return 1;
}

FSLI *VFS_FIND(char *name) {
	int n;
	
	for (n = 0; n < files_count; n++) {
		FSLI *slice = &files[n];
		if (stricmp(name, slice->name) == 0) {
			#ifdef DEBUG_VFS
				printf("VFS_FIND('%s', %d)\n", name, n);
			#endif
			return slice;
		}
	}
	
	return NULL;
}

/* Cache PAK file handles using FILE* directly (bypasses SDL2 Android overhead) */
#define VFS_MAX_PAKS 8
static FILE *vfs_cached_files[VFS_MAX_PAKS] = {0};
static char vfs_cached_paths[VFS_MAX_PAKS][512] = {0};
static int vfs_pak_count = 0;

static FILE *VFS_GET_PAK_HANDLE(const char *path) {
	int n;
	/* Check if we already have this PAK open */
	for (n = 0; n < vfs_pak_count; n++) {
		if (stricmp(vfs_cached_paths[n], path) == 0 && vfs_cached_files[n]) {
			return vfs_cached_files[n];
		}
	}
	/* Not found - open it */
	if (vfs_pak_count >= VFS_MAX_PAKS) {
		/* Cache full - close oldest */
		if (vfs_cached_files[0]) {
			fclose(vfs_cached_files[0]);
		}
		memmove(&vfs_cached_files[0], &vfs_cached_files[1], 
			(VFS_MAX_PAKS - 1) * sizeof(FILE*));
		memmove(&vfs_cached_paths[0], &vfs_cached_paths[1],
			(VFS_MAX_PAKS - 1) * 512);
		vfs_pak_count--;
	}
	FILE *f = fopen(path, "rb");
	if (f) {
		strncpy(vfs_cached_paths[vfs_pak_count], path, 511);
		vfs_cached_files[vfs_pak_count] = f;
		vfs_pak_count++;
	}
	return f;
}

int VFS_READ(FSLI *slice, uint8_t *buffer) {
	if (!slice) return -1;
	FILE *f = VFS_GET_PAK_HANDLE(slice->pak->path);
	if (f != NULL) {
		fseek(f, slice->pos, SEEK_SET);
		fread(buffer, 1, slice->len, f);
		return slice->len;
	} else {
		PROGRAM_EXIT_ERROR("Can't open pak '%s'", slice->pak->path);
	}
	return 0;
}

/* Optimized LZ decompressor (defined in lz_decompress_arm.c) */
extern void LZ_UNCOMPRESS_OPT(uint8_t *input, uint32_t input_length, uint8_t *output, uint32_t *output_length);

void LZ_UNCOMPRESS(uint8_t *input, uint32_t input_length, uint8_t *output, uint32_t *output_length) {
	LZ_UNCOMPRESS_OPT(input, input_length, output, output_length);
}

SDL_RWops *VFS_LOAD_EX(char *name) {
	int loaded = 0;
	FSLI cfsli = {0};
	FSLI *fsli = &cfsli;
	#ifdef __ANDROID__
	Uint32 _t0 = SDL_GetTicks();
	#endif
	
	/*#ifdef CHECK_FILESYSTEM	
	{
		char zname[0x100];
		sprintf(zname, "PAK/%s", name);
		
		if (_file_exists(zname)) {
			SDL_RWops *rw;
			#ifdef DEBUG_VFS
				printf("Using filesystem file ('%s')\n", zname);
			#endif

			rw = SDL_RWFromFile(zname, "rb");
			if (rw) {
				cfsli.len = SDL_RWseek(rw, 0, SEEK_END);
				cfsli.pos = 0;
				SDL_RWseek(rw, 0, SEEK_SET);
				SDL_RWread(rw, buffer, cfsli.len, 1);
				SDL_RWclose(rw);
				loaded = 1;
				#ifdef DEBUG_VFS
					printf("Loaded\n");
				#endif
			} else {
				#ifdef DEBUG_VFS
					printf("Can't load file\n");
				#endif
			}
		}
	}
	#endif*/
	
	if (!loaded) {
		if (!(fsli = VFS_FIND(name))) return NULL;
		
		#ifdef COMP_BUFFER_DYN
			temp1_update(fsli->len);
			//temp2_update(0);
		#endif
			
		VFS_READ(fsli, temp1);
	}
	
	if (temp1[0] == 'L' || temp1[1] == 'Z') {
		int size_c, size_u;
		size_c = mem4(temp1 + 2 + 0);
		size_u = mem4(temp1 + 2 + 4);


		#ifdef COMP_BUFFER_DYN
			temp2_update(size_u + 1);
		#endif		
		
		LZ_UNCOMPRESS(temp1 + 10, size_c, temp2, NULL);
		
		#ifdef COMP_BUFFER_DYN
			temp1_update(0);
		#endif
		
		temp2[size_u] = 0;
		#ifdef __ANDROID__
		printf("VFS_LOAD_EX('%s'): %d ms (LZ decompressed %d -> %d)\n", name, SDL_GetTicks() - _t0, size_c, size_u);
		#endif
		return SDL_RWFromMem(temp2, size_u);
	} else {		
		#ifdef __ANDROID__
		printf("VFS_LOAD_EX('%s'): %d ms (raw %d bytes)\n", name, SDL_GetTicks() - _t0, fsli->len);
		#endif
		return SDL_RWFromMem(temp1, fsli->len);
	}
}

SDL_RWops *VFS_LOAD(char *name) {
	SDL_RWops *r;
	READING_START();
	r = VFS_LOAD_EX(name);
	READING_END();
	return r;
}

SDL_RWops *STREAM_UNCOMPRESS(SDL_RWops *f) {
	int size_c;
	size_c = SDL_RWseek(f, 0, SEEK_END);
	
	#ifdef COMP_BUFFER_DYN
		temp1_update(size_c);
		//temp2_update(0);
	#endif
	
	SDL_RWseek(f, 0, SEEK_SET);
	SDL_RWread(f, temp1, 1, size_c);
	SDL_RWclose(f);

	if (temp1[0] == 'L' || temp1[1] == 'Z') {
		int size_c, size_u;
		size_c = mem4(temp1 + 2 + 0);
		size_u = mem4(temp1 + 2 + 4);

		#ifdef COMP_BUFFER_DYN
			temp2_update(size_u + 1);
		#endif

		LZ_UNCOMPRESS(temp1 + 10, size_c, temp2, NULL);

		#ifdef COMP_BUFFER_DYN
			temp1_update(0);
		#endif

		temp2[size_u] = 0;
		
		return SDL_RWFromMem(temp2, size_u);
	} else {		
		return SDL_RWFromMem(temp1, size_c);
	}
}

SDL_RWops *STREAM_UNCOMPRESS_MEM(void* start, int size) {
	return STREAM_UNCOMPRESS(SDL_RWFromConstMem(start, size));
}
