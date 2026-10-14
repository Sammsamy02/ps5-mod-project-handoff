# Step 3 delta — preserve readable game names

Game names containing Unicode and ordinary slash separators stay readable instead of becoming “Selected game.”

Changed only `web/installer-ui.js`, its owned test and this evidence. The path detector now requires a non-whitespace segment after an absolute slash. Quoted absolute paths, Windows paths, UUIDs and long opaque tokens still stay behind More info, where their original values remain available.

The regression first failed for `星 Café 🎮 / \ "0"`, then passed after the narrow pattern change. The focused browser suite passes **37/37**; its current result is [here](../step-02/test-results.json). The existing Step 2 note records the earlier committed 36-test checkpoint.

[1080p screenshot](screenshots/installer-unicode-name-1080.png). No changes to service operations, app routing or console behavior. No console or external network access.
