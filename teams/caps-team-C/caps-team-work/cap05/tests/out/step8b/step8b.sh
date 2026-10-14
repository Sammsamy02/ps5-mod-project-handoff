say "8b/9 final symbols for crash reports (cap05; Mac-only, reads the package, never changes the PRX; NON-FATAL)"
# Non-fatal on purpose: the PRX is already built and verified here, and labels are single-use.
# A failure only means crash reports for this build stay unsymbolized; it is written to
# $EV/FINAL-SYMBOLS-STATUS and can be redone by hand later (PATCH/APPLY.md step 4).
FSYM="$PKG_A/modbridge_supplier_composition.prx.final-symbols.json"
if [ -e "$FSYM" ]; then
  FSYM_STATUS="WARN: existing $FSYM left untouched (never overwritten); step skipped"
else
  python3 -I -B "$H/crashcap/final_symbols.py" --object "$PKG_A/pass-1/build/combined-supplier.o" \
    --bare "$PKG_A/pass-1/bare-original/modbridge_supplier_composition.prx" --packaged "$PRX" \
    --out "$FSYM" > "$EV/final-symbols.txt" 2>&1; rc=$?
  tail -1 "$EV/final-symbols.txt"
  if [ $rc -eq 0 ]; then
    FSYM_STATUS="PASS"; cp -p "$FSYM" "$EV/" 2>/dev/null
  else
    FSYM_STATUS="WARN: final_symbols rc=$rc (layout not reproduced; crash reports for this build would be unsymbolized; build result unaffected)"
  fi
fi
printf 'FINAL_SYMBOLS=%s\n' "$FSYM_STATUS" > "$EV/FINAL-SYMBOLS-STATUS"
say "8b/9 final symbols: $FSYM_STATUS"

