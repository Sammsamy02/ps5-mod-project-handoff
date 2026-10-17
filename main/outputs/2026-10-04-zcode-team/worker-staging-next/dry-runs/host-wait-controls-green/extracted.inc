#define MAX_DEP_NAME_LEN 64
#define MAX_DEPS         8
typedef struct {
	int stable_ms;
	int min_mod;
	int settle_ms;
} mode_params_t;

static pid_t proven_stable_pid = 0;

static struct timespec g_t0;

static long now_ms(void) {
	struct timespec n;
	clock_gettime(CLOCK_MONOTONIC, &n);
	return (n.tv_sec - g_t0.tv_sec) * 1000L + (n.tv_nsec - g_t0.tv_nsec) / 1000000L;
}

static mode_params_t get_mode_params(int mode) {
	if (mode == 1)
		return (mode_params_t) { .stable_ms = 150, .min_mod = 6, .settle_ms = 150 };
	else
		return (mode_params_t) { .stable_ms = 1000, .min_mod = 16, .settle_ms = 600 };
}

static int count_modules(pid_t pid) {
	int count = 0;
	for (uint32_t h = 0; h < 0x40; h++)
		if (kernel_dynlib_mapbase_addr(pid, h)) count++;
	for (uint32_t h = 0x2000; h < 0x2040; h++)
		if (kernel_dynlib_mapbase_addr(pid, h)) count++;
	return count;
}

static int dep_present(pid_t pid, const char* basename) {
	uint32_t h = 0;
	if (kernel_dynlib_handle(pid, basename, &h) != 0) return 0;
	return kernel_dynlib_mapbase_addr(pid, h) != 0;
}

static int deps_ready(pid_t pid, char needs[][MAX_DEP_NAME_LEN], int nneeds, int min_mod) {
	for (int i = 0; i < nneeds; i++)
		if (!dep_present(pid, needs[i])) return 0;
	if (!pt_resolve(pid, NID_LOADSTARTMODULE)) return 0;
	return count_modules(pid) >= min_mod;
}

static int wait_stable(pid_t pid, char needs[][MAX_DEP_NAME_LEN], int nneeds,
	const mode_params_t* mp, int max_ms) {
	const int tick_ms = 50;
	const int stable_req = (mp->stable_ms / tick_ms) < 2 ? 2 : (mp->stable_ms / tick_ms);
	const int max_ticks = max_ms / tick_ms;

	// (a) max_ms bounds the TICK COUNT, not wall time: every tick also pays
	// count_modules + deps_ready (~264 kernel calls). Measured on launch-10
	// (worker-ploader-70s): a full 500-tick burn takes ~60-70 s wall, not 25 s.
	// Enforce the intended wall budget so a slow kernel-call path cannot
	// stretch a "25 s" readiness wait past its design bound.
	const long deadline_ms = now_ms() + max_ms;

	// (b) cached readiness re-check: this exact process already PROVED
	// readiness earlier this boot, so re-validate it cheaply instead of
	// re-burning the full tick budget (the 500-tick burn plugin 1 already
	// paid must not be re-paid by plugin 2). The re-check runs the SAME
	// per-tick stability predicate, so genuine regression (module count
	// moved, deps dropped, process died) is caught here and falls through
	// to the full wait below; the cache never skips validation, it only
	// skips RE-proving what was proven and still holds.
	if (proven_stable_pid == pid && proven_stable_pid != 0) {
		const int cache_recheck_ticks = 4; // 4 predicates + ~200 ms sleep, vs 500 ticks
		int confirmed = 1;
		int prev = -1;
		for (int t = 0; t < cache_recheck_ticks; t++) {
			if (kill(pid, 0) != 0)
				return 0; // process died (unchanged failure semantics)
			int cur = count_modules(pid);
			if (prev >= 0 && cur != prev) {
				confirmed = 0; // module space moved again: not proven-stable
				break;
			}
			prev = cur;
			if (!deps_ready(pid, needs, nneeds, mp->min_mod)) {
				confirmed = 0; // stability regressed: fail closed
				break;
			}
			usleep(tick_ms * 1000);
		}
		if (confirmed)
			return 1; // proven-stable state still holds; burn skipped
		proven_stable_pid = 0; // stale: only a full revalidation may re-cache
	}

	int stable_hits = 0;
	int prev_count = -1;

	for (int t = 0; t < max_ticks; t++) {
		if (now_ms() >= deadline_ms)
			break; // wall budget exhausted; falls through to the same
			       // "try anyway if still alive" exit as tick exhaustion

		if (kill(pid, 0) != 0)
			return 0; // process died

		int cur = count_modules(pid);

		// reset stability counter if module count changed
		if (cur != prev_count && prev_count >= 0)
			stable_hits = 0;
		prev_count = cur;

		if (deps_ready(pid, needs, nneeds, mp->min_mod)) {
			if (++stable_hits >= stable_req) {
				usleep(mp->settle_ms * 1000); // wait for things to settle
				if (kill(pid, 0) != 0) return 0;

				// verify again after settle
				if (deps_ready(pid, needs, nneeds, mp->min_mod))
					{ proven_stable_pid = pid; return 1; } // ready! (PROVEN stable: cached for later plugins this boot)

				stable_hits = 0;
			}
		}
		else {
			stable_hits = 0;
		}

		usleep(tick_ms * 1000);
	}

	// try anyway if the process is still alive
	return kill(pid, 0) == 0 ? 1 : 0;
}
