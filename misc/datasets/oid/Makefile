F ?= random_walk
OUT ?= /tmp/oid_$(F).out

$(OUT): $(F).cc
	mkdir -p $(dir $@)
	c++ $< -o $@ -O2

run: $(OUT) dataset
	$(OUT) 1000000

clean:
	$(RM) -r /tmp/oid_*.out

dataset: oidv7-class-descriptions-boxable.csv oidv6-train-annotations-bbox.csv

oidv7-class-descriptions-boxable.csv:
	curl -o $@ https://storage.googleapis.com/openimages/v7/oidv7-class-descriptions-boxable.csv
oidv6-train-annotations-bbox.csv:
	curl -o $@ https://storage.googleapis.com/openimages/v6/oidv6-train-annotations-bbox.csv

.PHONY: run clean dataset
