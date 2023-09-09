#pragma once

#include <d3dx9tex.h>
#include <d3dx9.h>

namespace image
{
	struct size 
	{
		int width, height;
	};

	enum quality
	{
		low, high
	};

	struct image_t
	{
		IDirect3DTexture9* texture = nullptr;
		int width = 0, height = 0;
	};

	size maintain_aspect_ratio(int srcWidth, int srcHeight, int maxWidth, int maxHeight);

	bool create_text(PDIRECT3DTEXTURE9* out_texture, int* width, int* height, LPCVOID data_src, size_t data_size);

	bool create_text_high(PDIRECT3DTEXTURE9* out_texture, int* width, int* height, LPCVOID data_src, size_t data_size);

	image_t make_shared_image(const char* file_name, quality quality);
}
