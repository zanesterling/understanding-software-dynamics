.PHONY: runall runeach

runall: all
	./all

PERF_STAT=perf stat -e mem_load_retired.l1_miss,mem_load_retired.l2_miss,mem_load_retired.l3_miss

runeach: simple simple-columnwise
	$(PERF_STAT) ./simple
	$(PERF_STAT) ./simple-columnwise

all: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DALL -lm -o all

simple: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DSIMPLE -lm -o simple

simple-columnwise: interaction.cc ../util/measure.h
	gcc -O2 interaction.cc -DSIMPLE -lm -o simple-columnwise
