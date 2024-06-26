.PHONY: runall runeach clean

runall: all
	./all

PERF_STAT=perf stat -e mem_load_retired.l1_miss,mem_load_retired.l2_miss,mem_load_retired.l3_miss

runeach: transpose block-remap
	$(PERF_STAT) ./simple
	$(PERF_STAT) ./simple-columnwise
	$(PERF_STAT) ./transpose
	$(PERF_STAT) ./block-remap

runtrans: just-transpose just-transpose-88 just-transpose-88-flipped
	$(PERF_STAT) ./just-transpose > /dev/null
	$(PERF_STAT) ./just-transpose-88 > /dev/null
	$(PERF_STAT) ./just-transpose-88-flipped > /dev/null

runremap: just-block-remap
	$(PERF_STAT) ./just-block-remap

clean:
	rm -f simple simple-columnwise transpose just-transpose

all: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DALL -lm -o all

simple: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DSIMPLE -lm -o simple

simple-columnwise: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DSIMPLE_COLUMNWISE -lm -o simple-columnwise

just-transpose: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DJUST_TRANSPOSE -lm -o just-transpose

just-transpose-88: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DJUST_TRANSPOSE_88 -lm -o just-transpose-88

just-transpose-88-flipped: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DJUST_TRANSPOSE_88_2 -lm -o just-transpose-88-flipped

transpose: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DTRANSPOSE -lm -o transpose

just-block-remap: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DJUST_BLOCK_REMAP -lm -o just-block-remap

block-remap: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DBLOCK_REMAP -lm -o block-remap
