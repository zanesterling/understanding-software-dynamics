.PHONY: runall runeach clean

runall: all
	./all

PERF_STAT=perf stat -e mem_load_retired.l1_miss,mem_load_retired.l2_miss,mem_load_retired.l3_miss

runeach: simple simple-columnwise transpose
	$(PERF_STAT) ./simple
	$(PERF_STAT) ./simple-columnwise
	$(PERF_STAT) ./transpose

clean:
	rm -f simple simple-columnwise transpose

all: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DALL -lm -o all

simple: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DSIMPLE -lm -o simple

simple-columnwise: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DSIMPLE_COLUMNWISE -lm -o simple-columnwise

transpose: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DTRANSPOSE -lm -o transpose
