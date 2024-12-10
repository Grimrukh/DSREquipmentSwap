Native plugin that allows instant equipment swaps in Dark Souls: Remastered triggered by player SpEffects or weapon IDs. Compatible with Mod Engine 2.

Set up `(triggerID, idOffset)` pairs for left and right hand equipment in `DSRWeaponSwap.json`. The `idOffset` will be added to the equipped weapon ID in that hand when the trigger ID (either a weapon ID or SpEffect ID) is detected. Other settings (check intervals, cooldowns) can also be modified in the JSON. The included `DSRWeaponSwap.json` is a default template that will be copied to the DLL build directory if no such file already exists; your version should go next to your game executable if you are side-loading this plugin.

(Commission for Xenthos)