
#define DWMWA_MICA_EFFECT 1029

struct ACCENTPOLICY {
	int nAccentState;
	int nFlags;
	int nColor;
	int nAnimationId;
};

enum ACCENTFLAGS {
	ACCENT_FLAG_ENABLE_BLURBEHIND = 0x20,
	ACCENT_FLAG_ENABLE_ACRYLICBLURBEHIND = 0x80,
};

enum AccentState {
	ACCENT_DISABLED = 0,
	ACCENT_ENABLE_GRADIENT = 1,
	ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
	ACCENT_ENABLE_BLURBEHIND = 3,
	ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
	ACCENT_ENABLE_HOSTBACKDROP = 5,
	ACCENT_INVALID_STATE = 6
};

// Create the data structure for window composition attribute
struct WINDOWCOMPOSITIONATTRIBDATA {
	int nAttribute;
	PVOID pData;
	ULONG ulDataSize;
};

/// <summary>
/// SetWindowCompositionAttribute type definition 
/// </summary>
/// <param name="HWND"></param>
/// <param name="WINDOWCOMPOSITIONATTRIBDATA*"></param>
typedef HRESULT(WINAPI* pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);


typedef void (WINAPI* RtlGetVersion_FUNC) (OSVERSIONINFOEXW*);