/* Copyright (c) 2012-2014 Zeex
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SUBHOOK_H
#define SUBHOOK_H

#if defined _M_IX86 || defined __i386__
	#define SUBHOOK_X86
	#define SUBHOOK_BITS 32
#elif defined _M_AMD64 || __amd64__
	#define SUBHOOK_X86_64
	#define SUBHOOK_BITS 64
#else
	#error Unsupported architecture
#endif

#if defined __linux__
	#define SUBHOOK_LINUX
#elif defined _WIN32
	#define SUBHOOK_WINDOWS
#else
	#error Unsupported operating system
#endif

#if !defined SUHOOK_EXTERN
	#if defined __cplusplus
		#define SUBHOOK_EXTERN extern "C"
	#else
		#define SUBHOOK_EXTERN extern
	#endif
#endif

#if defined SUBHOOK_STATIC
	#define SUBHOOK_API
	#define SUBHOOK_EXPORT SUBHOOK_EXTERN
#endif

#if !defined SUBHOOK_API
	#if defined SUBHOOK_X86
		#if defined SUBHOOK_WINDOWS
			#define SUBHOOK_API __cdecl
		#elif defined SUBHOOK_LINUX
			#define SUBHOOK_API __attribute__((cdecl))
		#endif
	#else
		#define SUBHOOK_API
	#endif
#endif

#if !defined SUBHOOK_EXPORT
	#if defined SUBHOOK_WINDOWS
		#if defined SUBHOOK_IMPLEMENTATION
			#define SUBHOOK_EXPORT SUBHOOK_EXTERN __declspec(dllexport)
		#else
			#define SUBHOOK_EXPORT SUBHOOK_EXTERN __declspec(dllimport)
		#endif
	#elif defined SUBHOOK_LINUX
		#if defined SUBHOOK_IMPLEMENTATION
			#define SUBHOOK_EXPORT SUBHOOK_EXTERN __attribute__((visibility("default")))
		#else
			#define SUBHOOK_EXPORT SUBHOOK_EXTERN
		#endif
	#endif
#endif

struct subhook;
typedef struct subhook *subhook_t;

SUBHOOK_EXPORT subhook_t SUBHOOK_API subhook_new(void *src, void *dst);
SUBHOOK_EXPORT void SUBHOOK_API subhook_free(subhook_t hook);

SUBHOOK_EXPORT void *SUBHOOK_API subhook_get_src(subhook_t hook);
SUBHOOK_EXPORT void *SUBHOOK_API subhook_get_dst(subhook_t hook);
SUBHOOK_EXPORT void *SUBHOOK_API subhook_get_trampoline(subhook_t hook);

SUBHOOK_EXPORT int SUBHOOK_API subhook_install(subhook_t hook);
SUBHOOK_EXPORT int SUBHOOK_API subhook_is_installed(subhook_t hook);
SUBHOOK_EXPORT int SUBHOOK_API subhook_remove(subhook_t hook);

/* Reads hook destination address from code.
 *
 * This is useful when you don't know the address or want to check
 * whether src is already hooked.
 */
SUBHOOK_EXPORT void *SUBHOOK_API subhook_read_dst(void *src);

#ifdef __cplusplus

class SubHook
{
public:
	SubHook() : hook_(0) {}
	SubHook(void *src, void *dst) : hook_(subhook_new(src, dst)) {}

	~SubHook() {
		subhook_remove(hook_);
		subhook_free(hook_);
	}

	void *GetSrc() { return subhook_get_src(hook_); }
	void *GetDst() { return subhook_get_dst(hook_); }
	void *GetTrampoline() { return subhook_get_trampoline(hook_); }

	bool Install() {
		return subhook_install(hook_) >= 0;
	}

	bool Install(void *src, void *dst) {
		if (hook_ == 0) {
			hook_ = subhook_new(src, dst);
		}
		return Install();
	}

	bool Remove() {
		return subhook_remove(hook_) >= 0;
	}

	bool IsInstalled() const {
		return !!subhook_is_installed(hook_);
	}

	class ScopedRemove
	{
	public:
		ScopedRemove(SubHook *hook)
			: hook_(hook)
			, removed_(hook_->Remove())
		{
		}

		~ScopedRemove() {
			if (removed_) {
				hook_->Install();
			}
		}

	private:
		ScopedRemove(const ScopedRemove &);
		void operator=(const ScopedRemove &);

	private:
		SubHook *hook_;
		bool removed_;
	};

	class ScopedInstall
	{
	public:
		ScopedInstall(SubHook *hook)
			: hook_(hook)
			, installed_(hook_->Install())
		{
		}

		~ScopedInstall() {
			if (installed_) {
				hook_->Remove();
			}
		}

	private:
		ScopedInstall(const ScopedInstall &);
		void operator=(const ScopedInstall &);

	private:
		SubHook *hook_;
		bool installed_;
	};

	static void *ReadDst(void *src) {
		return subhook_read_dst(src);
	}

private:
	SubHook(const SubHook &);
	void operator=(const SubHook &);

private:
	subhook_t hook_;
};

#endif /* __cplusplus */

#endif /* SUBHOOK_H */
