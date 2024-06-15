// Code for Chapter 2: Measuring CPUs.

#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <x86intrin.h>

size_t N_ITERATIONS = 1000 * 1000 * 1000;

// Always returns false, but the compiler doesn't know that.
// Used to mark variables live.
bool NeverTrue() { return 0 == time(NULL); }

void usage(FILE* out_file, const char* const prog_name) {
	fprintf(
		out_file,
		"usage:\n"
		"\t%s [-b BASE_CLOCK_SPEED -t TRUE_CLOCK_SPEED]\n"
		"\n"
		"args:\n"
		"\t-b BASE_CLOCK_SPEED: The unmodified speed of your CPU clock.\n"
		"\t-t TRUE_CLOCK_SPEED: The actual speed of your CPU clock,\n"
		"\t                     accounting for overclocking.\n"
		"\n"
		"examples:\n"
		"\t%s\n"
		"\t%s -b 3.6 -t 4.65\n",
		prog_name,
		prog_name,
		prog_name
	);
}

struct Args {
	double clock_multiplier;
};

// Parses str as a double, setting the result in *out_ptr.
//
// Expects the whole string to be parsed.
// If parsing fails, does not modify *out_ptr.
//
// Returns true if parsing succeeded, false otherwise.
bool ParseDoubleOrFail(const char* str, double* out_ptr) {
	char* end_ptr = NULL;
	double val = strtod(str, &end_ptr);
	if (*end_ptr != '\0') {
		return false;
	}
	// TODO: check errno
	*out_ptr = val;
	return true;
}

Args ParseArgs(int argc, char** argv) {
	bool base_clock_set = false;
	bool true_clock_set = false;
	double base_clock_speed = 1;
	double true_clock_speed = 1;
	int opt;
	while (-1 != (opt = getopt(argc, argv, "b:t:"))) {
		switch (opt) {
			case 'b':
				if (!ParseDoubleOrFail(optarg, &base_clock_speed)) {
					fprintf(stderr, "failed to parse argument of -b as a float: \"%s\"\n", optarg);
					usage(stderr, argv[0]);
					exit(EXIT_FAILURE);
				}
				base_clock_set = true;
				break;

			case 't':
				if (!ParseDoubleOrFail(optarg, &true_clock_speed)) {
					fprintf(stderr, "failed to parse argument of -t as a float: \"%s\"\n", optarg);
					usage(stderr, argv[0]);
					exit(EXIT_FAILURE);
				}
				true_clock_set = true;
				break;

			default:
				usage(stderr, argv[0]);
				exit(EXIT_FAILURE);
		}
	}
	if (base_clock_set != true_clock_set) {
		fprintf(stderr, "You must set both -b and -t, or neither one.\n");
		usage(stderr, argv[0]);
		exit(EXIT_FAILURE);
	}

	Args args = {};
	args.clock_multiplier = true_clock_speed / base_clock_speed;
	return args;
}

int main(int argc, char** argv) {
	Args args = ParseArgs(argc, argv);

	// Get bits unknown by the compiler to avoid precalculating the sum.
	// Mark volatile to avoid turning the loop into an imul.
	volatile uint64_t iter = time(NULL) & 0xff;
	uint64_t sum = 0;

	// I'm using these mfences as nops to push the for loop into a 32B-aligned
	// region of memory. This ensures that each loop iteration only needs one
	// code fetch cycle, preventing the fetch from becoming a bottleneck.
	//
	// You will need to inspect the resulting ASM on your machine and adjust
	// alignment manually.
	//
	// PS: Why doesn't the compiler do this for us?
	_mm_mfence();
	_mm_mfence();
	_mm_mfence();
	_mm_mfence();
	_mm_mfence();
	_mm_mfence();
	int64_t start_cy = __rdtsc();
	for (size_t i = 0; i < N_ITERATIONS; ++i) {
		sum += iter;
	}
	int64_t stop_cy = __rdtsc();
	int64_t elapsed = stop_cy - start_cy;

	fprintf(
		stdout,
		"iters:       %lu\n"
		"cy reported: %ld\n"
		"cy adjusted: %f\n"
		"cy/iter:     %f\n",
		N_ITERATIONS,
		elapsed,
		elapsed * args.clock_multiplier,
		1.0 * elapsed / N_ITERATIONS
	);

	// Mark sum and iter live.
	if (NeverTrue()) fprintf(stdout, "%lu %lu\n", sum, iter);

	return 0;
}
