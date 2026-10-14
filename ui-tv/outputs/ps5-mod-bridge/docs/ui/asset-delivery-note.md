# UI asset delivery

The controller keyboard and installer preview now reach the browser through both existing app-serving paths, so the new screens can actually load their dependencies.

Changed files are `companion/modbridge_host.py` (static-serving block only), `native-host/web_assets.py`, new `tests/ui_asset_delivery_tests.py`, and this note. No source web asset, runtime, native HTTP route, game, mod or console file was changed by this worker.

The desktop host adds exact static routes for `controller-keyboard.js`, `config.js`, `installer-service.js`, `installer-mock.js` and `installer-ui.js`. They use the same content type, origin checks and CSP as the existing app script and need no Nexus login. An unknown non-API `.js` route now returns 404 before account/session handling. API handlers, session/CSRF behavior and installer backend availability are unchanged; this does not implement new installer HTTP endpoints.

The native asset generator still returns exactly `index.html`, `app.js`, and `style.css`, and emits the same three C symbols. It parses the source index and reads only its explicitly referenced, allowlisted classic scripts. Their bytes are joined in exact source order with statement/newline separators. Extra module script tags are removed only from the generated index; its single existing `/app.js` reference loads the bundle. The source index and modules remain unchanged. Thus config precedes service, service precedes mock, mock precedes installer UI, and those modules precede app startup when declared in that order by the source index. The controller keyboard is also declared before app startup.

At this delivery step, the actual source index declares `/controller-keyboard.js` then `/app.js`, both deferred; the emitted index retains only `/app.js`, whose bundle contains those two sources in that order. Desktop serves those paths unchanged. Root's later installer wiring will declare `/config.js`, `/installer-service.js`, `/installer-mock.js`, `/installer-ui.js` between the keyboard and app. The focused full-module fixture already verifies that exact six-script order and dependency binding; the generator does not assume unreferenced modules should be included.

Legacy indexes containing only `/app.js` retain their exact three original byte streams. Missing modules, unknown/external/traversal references, inline content, duplicate references and nonordinary script modes refuse generation. Every read retains regular-file, non-symlink, UTF-8, no-NUL and byte-size checks. The final three-asset bundle keeps the existing 256 KiB total bound. No extra native HTTP routes or embedded symbols are introduced.

Verification: **9/9 new delivery tests pass**, including real desktop loopback HTTP GET of all five added modules without an account/provider, unknown JavaScript 404, cross-origin rejection, native source-order execution/binding, actual keyboard source in the generated bundle, three-symbol emission, source-byte preservation and malformed/missing/symlink/encoding/size refusals. Existing pure Python `native-host/tests/test_web_assets.py` also passes **3/3**. Python compilation and whitespace checks pass.

Commands:

```text
python3 -m unittest discover -s tests -p ui_asset_delivery_tests.py -v
python3 -m unittest discover -s native-host/tests -p test_web_assets.py -v
```

The existing `native-host/tests/test_app_assets.py` was read but not run: its setup compiles native fixtures, outside this slice's explicit no-native-build scope. It retains its original three-file fixtures and no test was bypassed or edited. Root owns integrated UI/project regression verification. All network traffic in these focused tests stays on an owned loopback fixture; no provider or console was contacted, and no native binary was built.
