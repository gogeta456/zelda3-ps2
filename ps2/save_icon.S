# Default memory card icon (see make_save_icon.py), copied to the save folder on the first save.
    .section .rodata
    .align 4
    .global kSaveIconSys
    .global kSaveIconSysEnd
    .global kSaveIcon
    .global kSaveIconEnd
kSaveIconSys:
    .incbin "ps2/icon.sys"
kSaveIconSysEnd:
    .align 4
kSaveIcon:
    .incbin "ps2/zelda3.icn"
kSaveIconEnd:
