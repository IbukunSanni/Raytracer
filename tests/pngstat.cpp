// Prints the min and max RGB byte in a PNG, so a shell test can assert on
// a render without a PNG decoder of its own.
//
//     pngstat tests/out/furnace_full.png     ->  "min 255 max 255"
//
// A uniform image has min == max, which is how the furnace test checks the
// sphere is invisible against its environment.

#include <lodepng/lodepng.h>

#include <cstdio>
#include <vector>

int main(int argc, char ** argv) {
	if (argc != 2) {
		std::fprintf(stderr, "usage: pngstat <file.png>\n");
		return 2;
	}

	std::vector<unsigned char> img;
	unsigned w = 0, h = 0;
	if (unsigned err = lodepng::decode(img, w, h, argv[1])) {
		std::fprintf(stderr, "pngstat: %s: %s\n", argv[1], lodepng_error_text(err));
		return 2;
	}

	int lo = 255, hi = 0;
	for (size_t i = 0; i + 3 < img.size(); i += 4) {   // RGBA, alpha skipped
		for (int c = 0; c < 3; ++c) {
			const int v = img[i + (size_t) c];
			if (v < lo) lo = v;
			if (v > hi) hi = v;
		}
	}

	std::printf("min %d max %d\n", lo, hi);
	return 0;
}
