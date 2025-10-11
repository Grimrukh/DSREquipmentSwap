# DSR Equipment Swap

`Version 0.1.0`

Native plugin that allows instant equipment swaps in Dark Souls: Remastered.

Triggers are read from `DSREquipmentSwap.json`. Each swap can be triggered by a SpEffect ID on the player and/or a 
specific param ID currently equipped. One of these can be omitted (value -1 or 0) but not both. When the trigger is
detected, the equipment in that slot is swapped to the item with the specified ID offset added to the currently 
equipped item ID. The bundled `DSREquipmentSwap.json` has some example triggers for the left-handed weapon. Each swap
has three required parameters:

- `SpEffectIDTrigger`: The ID of the SpEffect that will trigger the swap. Set to -1 or 0 to ignore this trigger.
- `ParamIDTrigger`: The ID of the param that will trigger the swap. Set to -1 or 0 to ignore this trigger.
- `IDOffset`: The offset to add to the currently equipped item ID to get the new item ID to equip.
- `IsPermanent`: If true, the swap will not be undone when the game reloads or (for weapons) the active handed weapon is
toggled. This value can be omitted and will default to false.

Note that a swap does NOT affect the player's inventory. It simply overwrites the ID of the current equipment in
memory. Manually equipping something else into that slot or unequipping (which, under the hood, just "equips fists" or
"equips naked skin") will naturally revert to the original item, which is desirable.

The only tricky case is weapons, which have a primary and secondary slot for each hand, only one of which is "active".
This plugin will intentionally undo a weapon swap if you change your active weapon slot in either hand, unless that
swap was set to `IsPermanent == true` in the JSON. Such swaps will only be undone when the player manually changes
equipment (under the hood, the plugin simply does not keep track of these slots for manual undoing later).

Another special case for weapons: `ParamIDTrigger` matches will be detected in the currently inactive slot for a hand,
but `SpEffectIDTrigger` matches will not. In other words, SpEffect triggers will never work for inactive weapons, so
you can use the same SpEffect ID to trigger different swaps on an *active* weapon with the same relative ID offset.

The plugin will also always revert a swap when the game reloads (dying, save-and-quit, etc.) unless that swap was
set to `IsPermanent == true`.

Some global settings are also exposed in the JSON that you can modify:

- `ProcessSearchTimeoutMs`: The maximum time to spend searching for the game process on startup or when lost.
- `ProcessSearchIntervalMs`: The interval between process search attempts.
- `MonitorIntervalMs`: The interval between checks for trigger conditions when the game is loaded.
- `GameLoadedIntervalMs`: The interval between checks for the game being loaded when currently not loaded.
- `SpEffectTriggerCooldownMs`: The minimum time between trigger activations for the same SpEffect ID (per swap).
If this is too low, a SpEffect that lasts a few frames (e.g. a TAE event) may trigger multiple swaps, depending on the
value of `MonitorIntervalMs`.

Compatible with [Mod Engine 2](https://www.nexusmods.com/darksoulsremastered/mods/790) and 
[Simplified Mod Engine 2](https://www.nexusmods.com/darksoulsremastered/mods/766).

The mod will generate a log file `DSREquipmentSwap.log` in the same directory as the executable, which can be useful for
debugging.

This mod is a commission for Xenthos (@Xenthalos).

## Notes

As this mod causes the player's current equipment to diverge from their inventory, there may be some corner case
situations that have not yet been detected. Only weapons have been thoroughly tested, but armor and rings should work 
as well as they are less complex to handle.
