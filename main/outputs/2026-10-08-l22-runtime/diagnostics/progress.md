# Diagnostics task ledger — plan: docs/superpowers/plans/2026-10-08-l22-runtime.md

Scope: item 4 only; local selected-source copies. Parent owns integration/rebuild/final review.
Ruling: task-local diagnostics directory is the ledger/test workspace instead of shared SDD scripts, because parent supplied exclusive output ownership and integration is concurrent.
Ruling: copied LuaUObject.hpp shared pusher error boundary approved by parent; copied LuaUScriptStruct.cpp is also needed for nested Font.Size lookup/set errors. No behavior repair.
Pre-flight: changed LuaUObject.hpp needs all dependent selected units rebuilt by parent; ABI header remains separately owned.
Task diagnostics: tests in progress; sources copied without changes.

Task diagnostics: complete — 23/23 error semantics assertions, 21/21 queue/drain/key callback assertions, 9/9 marker checks. Original hashes unchanged; candidate hashes match manifest. Parent owns target compile/import census and independent final review.
