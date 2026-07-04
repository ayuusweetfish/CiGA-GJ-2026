OUT ?= /tmp/oid_a.out

$(OUT): main.cc
	mkdir -p $(dir $@)
	c++ $< -o $@ -O2

run: $(OUT)
	$(OUT) 100000

clean:
	$(RM) -r "$(OUT)"

.PHONY: run clean
