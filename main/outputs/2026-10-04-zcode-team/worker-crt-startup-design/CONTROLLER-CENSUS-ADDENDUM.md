# Controller addendum: observed startup-symbol sets (from baseline-dll-census.json, dlls[].crt.startup_imports)

## ? (8 startup imports)
- `_initterm` — role: crt_initializer_table_walk
- `_initterm_e` — role: crt_initializer_table_walk
- `_crt_atexit` — role: atexit
- `_initialize_narrow_environment` — role: crt_env
- `_initialize_onexit_table` — role: atexit_table
- `_register_onexit_function` — role: atexit_table
- `_cexit` — role: crt_exit
- `_execute_onexit_table` — role: atexit_table

## ? (11 startup imports)
- `_initialize_narrow_environment` — role: crt_env
- `_initterm_e` — role: crt_initializer_table_walk
- `_initterm` — role: crt_initializer_table_walk
- `_cexit` — role: crt_exit
- `_crt_atexit` — role: atexit
- `_execute_onexit_table` — role: atexit_table
- `_register_onexit_function` — role: atexit_table
- `_initialize_onexit_table` — role: atexit_table
- `_errno` — role: crt_tls_errno
- `__stdio_common_vsprintf` — role: stdio_common
- `__stdio_common_vfprintf` — role: stdio_common
