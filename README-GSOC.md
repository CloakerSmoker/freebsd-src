
# Bootloader prompt editing, GSoC 2022 Project

## Deliverables

(strikethrough where completed, explanations for uncompleted below)

* ~~Required: Translation of modifier keys to terminal escapes under UEFI and legacy BIOS~~
* Required: Ability to configure key bindings via a conf file
* ~~Required: Support for common editing keys/shortcuts~~
* ~~Required: Support for command line history~~
* ~~Required: Support command/file/directory name completion~~
* ~~Required: Improved error messages and feedback (where feasible)~~
* ~~Optional: Lua function/variable name completion~~
* Optional: Flashing cursor for EFI (and any other tiny QoL improvements)

I didn't end up adding a specific conf file for keybindings because I realized that *any* script or file which is `include()`'d/ran by the bootloader can be used to customize bindings as long as I provide programmable interfaces for the key binding system.
So, instead of this deliverable, I implemented a Lua API for adding/removing keybinds, and ensured that the simple command based interface was comprehensive enough to customize bindings as well.

I didn't add a flashing cursor for EFI mainly because I got used to not having one. I think it is still worth thinking about this change, but it no longer fit into my project by the end.

Additionally, I exposed history manipulation to Lua, as well as the keybinding interface. In the end, this made for a very nice little scriptable system with support for Lua "hotkeys" or plain `key:action` bindings, alongside more complex behavior.

## Conclusion

I've completed the original task I set out to, and feel that I have improved the prompt's usability by a solid amount.

However, I also intended to try and wipe out some technical debt while I was at it. Unfortunately, I didn't end up feeling comfortable enough (or motivated enough) to really get knee-deep into the code and clean things up.
I think it was overambitious for me to expect that I would be able to spot tech debt (let alone fix it) without being more familiar with the code first. I'm not super confident now either, but I feel a lot less out of my depth than I did originally, and hope to come back to get my hands dirty with some proper debt.

## Future work

Beyond the EFI flashing cursor, there are some additional platform-specific changes that could be made in the future. Mainly, some platforms truncate input before sending it to the bootloader, and some don't support all of the terminal escapes used to draw/redraw the prompt line.
For example, U-Boot's console doesn't pass through the alt modifier, or home/end/ins/del. However, since support for those extra keys/escapes would require upstream changes, I've left that as future work (and included workaround keybindings which don't require alt/home/end/ins/del).

Also, like I mentioned, there's also the secret tech-dept goal that I had. I'm not sure where I'd start with this, but I think that it would be a good next step, and plan to keep it in mind for the future.
