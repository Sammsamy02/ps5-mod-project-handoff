# Next launch (combined): #12 polish + #11 sharp window + #10 C# + #7 folder check + keyboard check

**Status:** waiting for ONE decision from you (below). Everything else is prepared on the Mac: the #7 step 2 fix
passed its review and is in the build tree. The #12 polish is installed and verified on the PS5, and all other console prep is done (row-check 38/38).

## The decision that blocks the build

- The new build compiled and passed every check. Then it stopped at the final packaging step.
- The cause is small: the #10 package lists one helper script by its old fingerprint. The script itself is the
  correct, reviewed new version.
- I fixed that one line, but the automatic safety check stopped me, because changing a fingerprint can look like
  weakening a safety check. I undid my change and did not work around it.
- **To continue, tell me: "ok update the #10 pin".** Then I make that one-line change, rebuild (~25 min), upload,
  and turn on the mod loader.
- Details: `evidence/builds-20261012/R17-combined-cap10-cap11fu/PACKAGING-STOP-DECISION.md`.

## What this one launch tests

| # | What | What you do | What you should see |
|---|---|---|---|
| 12 | Leon polish: the glasses' glass and the IV bag are hidden instead of grey/white | play to the outside cutscene, look at Leon | no grey/white glasses lenses; clothes as before or better |
| 11 | The L1+R2 window is sharp at 4K | press L1+R2 | a big readable window, not tiny or blurry |
| 10 | C# runtime self-test | nothing | nothing on screen; I read it from the log |
| 7 | The game can see its own `Paks/~mods` folder (empty for now) | nothing | nothing on screen; I read it from the log |
| kb | Does the game see a USB keyboard? (optional) | if you have one: plug it in BEFORE starting the game, then in gameplay press F8, A, Space, Enter, left Shift, left Ctrl 3 times each, and click the left and right mouse buttons 3 times | nothing on screen; I read it from the log |
| all | All other mods still work, no stutter | play normally for a few minutes | everything works as in the last launches |

Then close the game and tell me "game closed".

## After this launch

1. If #7's folder check passes, the next launch tests Leon from `~mods` (no rebuild): I put the #12 install back to
   the original game files and put the unchanged Leon files in `~mods`. Leon should appear (checkered is expected,
   because that test has no #12 fix).
2. Then the Stellar Blade core blind test.
