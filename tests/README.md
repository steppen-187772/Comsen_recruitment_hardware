# Host tests

The test suite exercises the platform-independent calculator engine, result formatter, and touch
debounce filter on the host. It is intentionally separate from STM32 hardware integration testing.

Run locally:

```sh
mkdir -p build/host
gcc -std=c11 -Wall -Wextra -Werror -O0 --coverage \
  -Isubmission/Core/Inc \
  tests/test_core.c \
  submission/Core/Src/calculator_engine.c \
  submission/Core/Src/touch_filter.c \
  -lm -o build/host/core_tests
./build/host/core_tests
gcovr --filter 'submission/Core/Src/(calculator_engine|touch_filter)\.c' --fail-under-line 95
```

Hardware-dependent LCD and TTP229 behavior remains covered by the acceptance video and on-board
checklist in `submission/REPORT.md`.
