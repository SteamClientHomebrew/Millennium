#define _WINSOCKAPI_   

#include <stdafx.h>
#include <window/core/window.hpp>
#include <window/core/dx9_image.hpp>

#define STB_IMAGE_IMPLEMENTATION    
#define STB_IMAGE_RESIZE_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_resize.h>

image::size image::maintain_aspect_ratio(int srcWidth, int srcHeight, int maxWidth, int maxHeight)
{
	double ratio = min((static_cast<double>(maxWidth) / srcWidth), (static_cast<double>(maxHeight) / srcHeight));
	return { static_cast<int>(srcWidth * ratio), static_cast<int>(srcHeight * ratio) };
}

bool image::create_text(PDIRECT3DTEXTURE9* out_texture, int* width, int* height, LPCVOID data_src, size_t data_size)
{
	PDIRECT3DTEXTURE9 texture;
	int origWidth, origHeight, channels;

	stbi_uc* imageData = stbi_load_from_memory(static_cast<const stbi_uc*>(data_src), static_cast<int>(data_size), &origWidth, &origHeight, &channels, STBI_default);
	if (!imageData) {
		console.err("Failed to load image using stb_image.");
		return false;
	}

	//downscaling the image to a fifth of the original image size to save memory
	int halfWidth = origWidth / 5; int halfHeight = origHeight / 5;

	stbi_uc* downscaledData = new stbi_uc[halfWidth * halfHeight * channels];
	stbir_resize_uint8(imageData, origWidth, origHeight, 0, downscaledData, halfWidth, halfHeight, 0, channels);

	if (D3DXCreateTexture(*Window::getDevice(), halfWidth, halfHeight, 1, 0, D3DFMT_X8B8G8R8, D3DPOOL_MANAGED, &texture) != S_OK)
	{
		delete[] downscaledData;
		stbi_image_free(imageData);
		return false;
	}

	D3DLOCKED_RECT lockedRect;

	texture->LockRect(0, &lockedRect, NULL, 0);
	for (int y = 0; y < halfHeight; ++y)
	{
		memcpy((BYTE*)lockedRect.pBits + y * lockedRect.Pitch, downscaledData + y * halfWidth * channels, halfWidth * channels);
	}
	texture->UnlockRect(0);

	delete[] downscaledData;
	stbi_image_free(imageData);

	*out_texture = texture; *width = halfWidth; *height = halfHeight;
	return true;
}

bool image::create_text_high(PDIRECT3DTEXTURE9* out_texture, int* width, int* height, LPCVOID data_src, size_t data_size)
{
	PDIRECT3DTEXTURE9 texture;
	D3DXIMAGE_INFO image;

	if (D3DXCreateTextureFromFileInMemoryEx(*Window::getDevice(), data_src, data_size, D3DX_DEFAULT, D3DX_DEFAULT, 1, 0, D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0, &image, nullptr, &texture) != S_OK)
		return false;

	*out_texture = texture; *width = image.Width; *height = image.Height;

	return true;
}

image::image_t image::make_shared_image(const char* file_name, quality quality)
{
	PDIRECT3DTEXTURE9 out_texture = {};
	int height = 0;
	int width = 0;

	bool success = false; 

	try
	{
		std::vector<unsigned char> image_bytes = http::get_bytes(file_name);

		switch (quality)
		{
			case quality::low:
			{
				success = create_text(&out_texture, &width, &height, image_bytes.data(), image_bytes.size());
				break;
			}
			case quality::high:
			{
				success = create_text_high(&out_texture, &width, &height, image_bytes.data(), image_bytes.size());
				break;
			}
		}

		image_bytes.clear();
	}
	catch (const http_error& ex)
	{
		if (ex.code() == http_error::errors::couldnt_connect || ex.code() == http_error::errors::not_allowed)
		{
			success = false;
		}
	}

	//return nullptr if the image couldnt be created.
	return success ? image_t { out_texture, width, height } : image_t { nullptr, 0, 0 };
}