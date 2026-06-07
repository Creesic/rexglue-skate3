# FM2 Recompile Patches

This file documents FM2-specific fixes that live outside generated code.

## Fiber rexcrt mappings

`fm2_config.toml` maps the title's fiber helpers through `[rexcrt]`:

- `ConvertThreadToFiber = 0x82754CD0`
- `ConvertFiberToThread = 0x82754D60`
- `CreateFiber = 0x82754DB0`
- `DeleteFiber = 0x82754E90`
- `SwitchToFiber = 0x82754EF0`

This fixed the post-intro hang by letting rexglue handle the title's fiber transitions.

## Bad child slot guard

`sub_82603BE0` walks ten child pointers at `parent + 104 + index * 4` and calls the vtable slot at `child->vtable + 12`.

During racetrack loading, one of those child slots can retain a stale/freed pointer. The original generated code then faults while loading the child vtable. The permanent patch is a `[[midasm_hook]]` at `0x82603C28`, immediately after the child slot is loaded into `r11`.

The hook function is `FM2SkipBadChildSlot` in `src/fm2_hooks.cpp`. It checks that the child pointer, vtable slot, and call target are readable/aligned/plausible. If not, it clears the stale slot and asks codegen to jump to `0x82603C48`, which skips that child and continues the loop.

## Null nested vcall guard

`sub_8242A3C8` checks a flag byte at `object + 48`. If set, it loads a nested object from `object + 12` and calls vtable slot `+44`.

During later racetrack loading, that flag can be set while the nested object pointer is null. The generated code then faults while loading the nested vtable from guest address `0`.

The permanent patch is a `[[midasm_hook]]` at `0x8242A3D4`, immediately after `lwz r3,12(r3)`. The hook function is `FM2ReturnZeroOnBadNestedVcall` in `src/fm2_hooks.cpp`. If the nested object, vtable slot, or target is invalid, codegen jumps to `0x8242A3E8`, the function's existing `return 0` path.

## Bad list head guard

`sub_8234D348` can be called from the `sub_82603BE0` child walker through vtable slot `+12`. It loads a list head from `object + 20` and immediately dereferences it.

During racetrack loading, that list head can be null. The generated code then faults while reading guest address `0`.

The permanent patch is a `[[midasm_hook]]` at `0x8234D378`, immediately after `lwz r10,4(r28)`. The hook function is `FM2ReturnOnBadListHead` in `src/fm2_hooks.cpp`. If the list head is unreadable, codegen jumps to `0x8234D4EC`, the function's normal epilogue.

## Bad D5A8 list head guard

`sub_8234D5A8` walks another list rooted at `object + 20`. It loads the head into `r11` and then immediately dereferences it.

During racetrack loading, that head can be null. The generated code then faults while reading guest address `0`.

The permanent patch is a `[[midasm_hook]]` at `0x8234D5E4`, immediately after `lwz r11,4(r31)`. The hook function is `FM2ReturnOnBadD5A8ListHead` in `src/fm2_hooks.cpp`. If the list head is unreadable, codegen jumps to `0x8234D6B4`, the function's normal epilogue.

## Bad D4F8 list head guard

`sub_8234D4F8` is another vtable-dispatched list walk. It loads the head from `object + 20` into `r10` and immediately dereferences it.

During racetrack loading, that head can be null. The generated code then faults while reading guest address `0`.

The permanent patch is a `[[midasm_hook]]` at `0x8234D518`, immediately after `lwz r10,4(r31)`. The hook function is `FM2ReturnOnBadD4F8ListHead` in `src/fm2_hooks.cpp`. If the list head is unreadable, codegen jumps to `0x8234D590`, the function's normal epilogue.

## Bad 75A40 object guard

`sub_82375A40` receives an object pointer in `r4`, copies it to `r30`, and immediately reads and writes many fields between `object + 8` and `object + 168`.

At start-race time this pointer can be invalid and unaligned, causing the first field load at `object + 108` to fault.

The permanent patch is a `[[midasm_hook]]` at `0x82375A4C`, immediately after `mr r30,r4`. The hook function is `FM2ReturnOnBad75A40Object` in `src/fm2_hooks.cpp`. If the object range is unreadable, codegen jumps to `0x82375EC4`, the function's normal epilogue.

## Bad 76A58 active object guard

`sub_82376A58` loads the currently active object from `owner + 13140` into `r30`, then reads fields such as `object + 152`.

At start-race time this active object can be null. The generated code then faults while reading guest address `0x98`.

The permanent patch is a `[[midasm_hook]]` at `0x82376A68`, immediately after `lwz r30,13140(r31)`. The hook function is `FM2ReturnZeroOnBad76A58Object` in `src/fm2_hooks.cpp`. If the object range is unreadable, it sets the return value path to zero and codegen jumps to `0x82376BF8`, the function's normal return tail.

## Bad 75ED0 mask guard

`sub_82375ED0` receives a mask/payload pointer in `r4`, copies it to `r25`, and immediately reads fields such as `mask + 24`, `mask + 32`, and later fields through `mask + 120`.

At start-race time this pointer can be invalid and unaligned. The generated code then faults while reading guest address derived from that bad pointer.

The permanent patch is a `[[midasm_hook]]` at `0x82375EE0`, immediately after `mr r25,r4`. The hook function is `FM2ReturnOnBad75ED0Mask` in `src/fm2_hooks.cpp`. If the mask range is unreadable, codegen jumps to `0x823764A8`, the function's normal epilogue.

## Bad 40160 primary result guard

`sub_82540160` is reached from the finish-race teardown path through `sub_8254AB60`. It copies the first tracked object, calls that object's vtable slot `+160`, then copies the returned primary object into `r25` and immediately calls vtable slot `+20`.

When finishing a race, that returned primary object can be null or stale. The generated code then faults while loading the vtable from `r25`.

The permanent patch is a `[[midasm_hook]]` at `0x825401D4`, immediately after `lwz r25,80(r1)`. The hook function is `FM2ReturnOnBad40160PrimaryResult` in `src/fm2_hooks.cpp`. If `r25` cannot safely call the required `+20` method or later release slot `+8`, codegen jumps to `0x8254038C`, the function's cleanup tail that releases the original `r24` object and returns.
