.DEFAULT_GOAL := all
CC = cc
AR ?= ar
MODE ?= debug
CPPFLAGS += -Iinclude -D_XOPEN_SOURCE=700 -D_FILE_OFFSET_BITS=64
WARNINGS = -Wall -Wextra -Wpedantic -Wconversion -Wshadow -Wformat=2 -Wstrict-prototypes -Wmissing-prototypes -Werror
CFLAGS += -std=c17 $(WARNINGS)
CFLAGS_debug = -O0 -g3
CFLAGS_release = -O2 -g
CFLAGS_asan = -O1 -g3 -fsanitize=address -fno-omit-frame-pointer -fno-pie
CFLAGS_ubsan = -O1 -g3 -fsanitize=undefined -fno-sanitize-recover=all -fno-omit-frame-pointer
CFLAGS += $(CFLAGS_$(MODE))
LDFLAGS_asan = -fsanitize=address -no-pie
LDFLAGS_ubsan = -fsanitize=undefined
LDFLAGS += $(LDFLAGS_$(MODE))
LDLIBS += -lm
SOURCES := $(shell find src -name '*.c' ! -name main.c | sort)
OBJECTS := $(patsubst %.c,build/$(MODE)/%.o,$(SOURCES))
TEST_OBJECTS := $(patsubst %.c,build/$(MODE)/testlib/%.o,$(SOURCES))
TEST_SOURCES := $(wildcard tests/unit/*.c tests/integration/*.c) tests/test_main.c tests/test_support.c
TEST_RUN_OBJECTS := $(patsubst %.c,build/$(MODE)/%.o,$(TEST_SOURCES))
LIBRARY := build/$(MODE)/libcdb.a
.PHONY: all debug release test stress benchmark clean sanitize test-asan test-ubsan valgrind format lint
all: bin/cdb
bin/cdb: bin/cdb-$(MODE) FORCE
	cp $< $@
bin/cdb-$(MODE): build/$(MODE)/src/main.o $(LIBRARY)
	@mkdir -p bin
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
$(LIBRARY): $(OBJECTS)
	$(AR) rcs $@ $^
build/$(MODE)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) -Itests $(CFLAGS) -MMD -MP -c $< -o $@
build/$(MODE)/testlib/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DCDB_TEST_FAULTS -MMD -MP -c $< -o $@
bin/test-cdb-$(MODE): $(TEST_RUN_OBJECTS) $(TEST_OBJECTS)
	@mkdir -p bin
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
bin/stress-cdb-$(MODE): build/$(MODE)/tests/stress/test_large_dataset.o build/$(MODE)/tests/test_support.o $(LIBRARY)
	@mkdir -p bin
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
BENCH_SOURCES := $(wildcard benchmarks/*.c)
bin/benchmark-cdb-$(MODE): $(patsubst %.c,build/$(MODE)/%.o,$(BENCH_SOURCES)) $(LIBRARY)
	@mkdir -p bin
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@
test: all bin/test-cdb-$(MODE)
	./bin/test-cdb-$(MODE)
	sh scripts/cli-test.sh ./bin/cdb
stress: bin/stress-cdb-$(MODE)
	./bin/stress-cdb-$(MODE) $(ROWS)
benchmark: bin/benchmark-cdb-$(MODE)
	./bin/benchmark-cdb-$(MODE) $(ROWS)
debug:
	$(MAKE) MODE=debug all
release:
	$(MAKE) MODE=release all
test-asan:
	ASAN_OPTIONS=$${ASAN_OPTIONS:-detect_leaks=1:halt_on_error=1} $(MAKE) MODE=asan test stress ROWS=10000
test-ubsan:
	UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 $(MAKE) MODE=ubsan test stress ROWS=10000
sanitize:
	$(MAKE) test-asan
	$(MAKE) test-ubsan
valgrind:
	@command -v valgrind >/dev/null || { echo 'Install Valgrind to run this target'; exit 1; }
	$(MAKE) MODE=debug bin/test-cdb-debug
	CDB_SKIP_CRASH_TESTS=1 valgrind --leak-check=full --show-leak-kinds=all --errors-for-leak-kinds=all --error-exitcode=1 ./bin/test-cdb-debug
format:
	@command -v clang-format >/dev/null || { echo 'Install clang-format to run this target'; exit 1; }
	find include src tests benchmarks examples -name '*.[ch]' -exec clang-format -i {} +
lint:
	@command -v gcc >/dev/null || { echo 'GCC is required for -fanalyzer'; exit 1; }
	@set -e; for file in $(SOURCES) src/main.c; do gcc $(CPPFLAGS) -std=c17 $(WARNINGS) -fanalyzer -c "$$file" -o /dev/null; done
clean:
	rm -rf build bin
.PHONY: FORCE
FORCE:
-include $(OBJECTS:.o=.d) $(TEST_OBJECTS:.o=.d) $(TEST_RUN_OBJECTS:.o=.d) build/$(MODE)/src/main.d
