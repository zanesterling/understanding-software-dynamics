// Want to measure:
// - cache line size
// - total size of each level of cache
// - associativity of each level of cache

#include <stdint.h>
#include <cstdio>

void ClearCache() {
}

uint16_t MeasureCacheLineSize() { return 0; }

struct LevelSizes {
	uint64_t l1;
	uint64_t l2;
	uint64_t l3;
};
LevelSizes MeasureCacheLevelSizes() { return {0, 0, 0}; }

struct LevelAssocs {
	uint16_t l1;
	uint16_t l2;
	uint16_t l3;
};
LevelAssocs MeasureCacheLevelAssocs() { return {0, 0, 0}; }

int main(int argc, char** argv) {
	uint16_t line_size = MeasureCacheLineSize();
	printf("line size: %uB\n", line_size);
	printf("\n");

	LevelSizes level_sizes = MeasureCacheLevelSizes();
	printf("level sizes:\n");
	printf("\tL1: %luB\n", level_sizes.l1);
	printf("\tL2: %luB\n", level_sizes.l2);
	printf("\tL3: %luB\n", level_sizes.l3);
	printf("\n");

	LevelAssocs level_assocs = MeasureCacheLevelAssocs();
	printf("level assocs:\n");
	printf("\tL1: %uB\n", level_assocs.l1);
	printf("\tL2: %uB\n", level_assocs.l2);
	printf("\tL3: %uB\n", level_assocs.l3);
	printf("\n");

	return 0;
}
