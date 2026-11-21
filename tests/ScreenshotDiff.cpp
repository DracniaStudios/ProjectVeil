// veil_screenshot_diff: compares two PNG screenshots (parity harness, B-14).
//
//   veil_screenshot_diff <a.png> <b.png> [--ignore x,y,w,h]...
//
// Both images are loaded as 8-bit RGBA. Pixels inside an --ignore rectangle
// (pixels, origin top-left) are left out of every figure. Prints both sizes,
// the mean absolute error per channel and the percentage of compared pixels
// where any channel differs by more than kChannelTolerance.
//
// Exit 0: PASS. Exit 1: FAIL (sizes differ or a threshold is exceeded).
// Exit 2: bad arguments or an unreadable file.
//
// Self-contained: only stb_image, no other library.

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {
	constexpr int kChannelTolerance = 8;          // a pixel is "over" when any channel differs by more than this
	constexpr double kMaxOverPercent = 0.5;       // at most this percentage of compared pixels may be over
	constexpr double kMaxMeanError = 1.0;         // mean absolute error averaged over the four channels

	struct Rect { long long x, y, w, h; };

	struct Image
	{
		unsigned char* data = nullptr;
		int w = 0, h = 0;
		~Image() { if (data) stbi_image_free(data); }
	};

	bool ParseIgnore(const char* text, Rect& out)
	{
		long long v[4];
		const char* p = text;
		for (int i = 0; i < 4; ++i)
		{
			if (*p == '\0') return false;
			char* end = nullptr;
			v[i] = std::strtoll(p, &end, 10);
			if (end == p) return false;
			p = end;
			if (i < 3) { if (*p != ',') return false; ++p; }
		}
		if (*p != '\0') return false;
		if (v[2] < 0 || v[3] < 0) return false;
		out = { v[0], v[1], v[2], v[3] };
		return true;
	}

	bool Load(const char* path, Image& img)
	{
		int comp = 0;
		img.data = stbi_load(path, &img.w, &img.h, &comp, 4);
		if (!img.data)
		{
			std::fprintf(stderr, "error: cannot read '%s': %s\n", path, stbi_failure_reason());
			return false;
		}
		return true;
	}

	int Usage()
	{
		std::fprintf(stderr, "usage: veil_screenshot_diff <a.png> <b.png> [--ignore x,y,w,h]...\n");
		return 2;
	}
}

int main(int argc, char** argv)
{
	std::vector<const char*> files;
	std::vector<Rect> ignores;
	for (int i = 1; i < argc; ++i)
	{
		if (std::strcmp(argv[i], "--ignore") == 0)
		{
			Rect r;
			if (i + 1 >= argc) { std::fprintf(stderr, "error: --ignore needs a value x,y,w,h\n"); return Usage(); }
			if (!ParseIgnore(argv[++i], r))
			{
				std::fprintf(stderr, "error: malformed --ignore value '%s' (want x,y,w,h with w,h >= 0)\n", argv[i]);
				return Usage();
			}
			ignores.push_back(r);
		}
		else if (std::strncmp(argv[i], "--", 2) == 0)
		{
			std::fprintf(stderr, "error: unknown option '%s'\n", argv[i]);
			return Usage();
		}
		else files.push_back(argv[i]);
	}
	if (files.size() != 2) return Usage();

	Image a, b;
	if (!Load(files[0], a) || !Load(files[1], b)) return 2;

	std::printf("A: %s  %d x %d\n", files[0], a.w, a.h);
	std::printf("B: %s  %d x %d\n", files[1], b.w, b.h);
	if (a.w != b.w || a.h != b.h)
	{
		std::printf("FAIL: sizes differ\n");
		return 1;
	}

	// Mark ignored pixels (rectangles clipped to the image)
	std::vector<unsigned char> skip(static_cast<size_t>(a.w) * a.h, 0);
	for (const Rect& r : ignores)
	{
		const long long x0 = std::max<long long>(0, r.x), y0 = std::max<long long>(0, r.y);
		const long long x1 = std::min<long long>(a.w, r.x + r.w), y1 = std::min<long long>(a.h, r.y + r.h);
		for (long long y = y0; y < y1; ++y)
			for (long long x = x0; x < x1; ++x)
				skip[static_cast<size_t>(y) * a.w + x] = 1;
	}

	unsigned long long sum[4] = { 0, 0, 0, 0 };
	unsigned long long compared = 0, over = 0;
	for (size_t p = 0; p < skip.size(); ++p)
	{
		if (skip[p]) continue;
		++compared;
		bool anyOver = false;
		for (int c = 0; c < 4; ++c)
		{
			const int d = std::abs(static_cast<int>(a.data[p * 4 + c]) - static_cast<int>(b.data[p * 4 + c]));
			sum[c] += static_cast<unsigned>(d);
			if (d > kChannelTolerance) anyOver = true;
		}
		if (anyOver) ++over;
	}

	std::printf("compared pixels: %llu of %zu (%zu ignored)\n", compared, skip.size(), skip.size() - static_cast<size_t>(compared));
	if (compared == 0)
	{
		std::printf("FAIL: every pixel is ignored, nothing to compare\n");
		return 1;
	}

	const char* names = "RGBA";
	double mean[4], total = 0.0;
	for (int c = 0; c < 4; ++c)
	{
		mean[c] = static_cast<double>(sum[c]) / static_cast<double>(compared);
		total += mean[c];
		std::printf("mean abs error %c: %.4f\n", names[c], mean[c]);
	}
	const double meanAll = total / 4.0;
	const double overPercent = 100.0 * static_cast<double>(over) / static_cast<double>(compared);
	std::printf("mean abs error (all channels): %.4f (limit %.2f)\n", meanAll, kMaxMeanError);
	std::printf("pixels with a channel off by more than %d: %llu = %.4f %% (limit %.2f %%)\n",
		kChannelTolerance, over, overPercent, kMaxOverPercent);

	if (overPercent > kMaxOverPercent)
	{
		std::printf("FAIL: %.4f %% of pixels over tolerance exceeds %.2f %%\n", overPercent, kMaxOverPercent);
		return 1;
	}
	if (meanAll > kMaxMeanError)
	{
		std::printf("FAIL: mean error %.4f exceeds %.2f\n", meanAll, kMaxMeanError);
		return 1;
	}
	std::printf("PASS: sizes match, %.4f %% over tolerance, mean error %.4f\n", overPercent, meanAll);
	return 0;
}
