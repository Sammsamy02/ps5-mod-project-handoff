# Townfall Mod Launch — Guide for the Console Owner

You are launching Townfall with a small mod tool attached. The controller prepares
everything on the console first. When the controller says "ready", your part takes
about two minutes.

## 1. What you'll see when the game launches

- Make sure Townfall is **fully closed** first (not suspended).
- The controller arms the loader tool (ploader). Then **you start Townfall normally**
  from the home screen. The launch is always done by you — nothing starts the game by itself.
- About **1 minute after the game starts**, the loader shows a short message on screen
  saying it is injecting, then reports **OK** or **FAIL**.
- **"OK" means:** the mod tool got into the running game — the first step worked.
  It does not yet prove everything works; the controller checks the game's hidden
  log files afterwards to confirm.

## 2. What the game should look like

Completely normal. The mod adds **no menus, pictures, sounds, or gameplay changes**
by itself. If the game looks and plays exactly like always, that is correct.
The only new thing is one extra menu you open on purpose (next section).

## 3. What to do

1. Play normally.
2. Once a **save is loaded**, press **OPTIONS** once — that opens the Item Spawner
   menu. Press OPTIONS again to close it.
3. Inside the menu: up/down moves the selection, CROSS picks. (OPTIONS acts like an
   "F7" key inside the game — that is all "F7" means in the notes.)

Testing without a save loaded is fine too, but expect **nothing on screen** —
the game just quietly notes "load a save first" in its log. That is normal, not a fault.

## 4. What success looks like

- The game's log contains this exact line (the controller collects the log — you
  don't have to find it):
  `[TownfallItemSpawner] Loaded v1.0. Clean pickup-only release. F7 opens the menu.`
- And the menu actually opens when you press OPTIONS with a save loaded.

If you saw the loader "OK" and the menu opens, the launch succeeded. Tell the
controller what you saw — that's all that's needed.

## 5. What failure looks like

Any of these:

- The game **doesn't start**, or the loader shows **FAIL** instead of OK.
- The game **crashes** (closes suddenly, or the console goes back to the home screen).
- The console itself freezes or becomes unresponsive.
- The controller later reports the "Loaded v1.0" line is **missing** from the log.

## 6. What to do if it fails

1. **Close the game.** Don't restart it, don't try again, don't press OPTIONS more —
   each launch is prepared for one attempt only, and retrying can make things worse.
2. **Write down what happened** while it's fresh: the exact message on screen
   (a photo of the popup is perfect), what you pressed, and roughly when.
3. **Tell the controller.** They will decide the next step.
4. If the console crashed: restarting it is **your call, in your hands**. Once it's
   back up, tell the controller **before** anything else is tried.

## 7. How long to wait

- After starting the game: give it **about 1 minute** for the loader's OK/FAIL message.
- After that: no waiting needed — just play normally and try the OPTIONS menu whenever
  you like. Nothing is time-sensitive after the first minute.

**In short:** close the game → controller arms the loader → you launch Townfall →
wait ~1 minute for "OK" → play normally → OPTIONS opens the Item Spawner menu in a
loaded save. If anything looks wrong: close the game, note it, tell the controller.
