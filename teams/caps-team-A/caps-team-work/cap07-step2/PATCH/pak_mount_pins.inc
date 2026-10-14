// GENERATED from evidence/pins.json by tools/gen_pins_inc.py — do not hand-edit.
// Per-version engine pin table (UE 5.6.1, image 3b6cfad47dd1).
// Two-source-verified (Epic source + decrypted executable); see pins.json.
inline ModBridge::PakMount::EnginePins modbridge_pak_pins_ue561()
{
    ModBridge::PakMount::EnginePins p;
    std::snprintf(p.image_sha256, sizeof p.image_sha256, "%s", "3b6cfad47dd17fea17d429bce80ae374d1fd2ef8894672cce63f5a26853622fc");
    p.pak_vtable = 0x7fee8c0;
    p.platform_mgr_get = 0xb8ed0;
    p.platform_mgr_getpf = 0xb8a70;
    p.slot_get_lower_level = 0x68;
    p.slot_delegate_execute = 0x60;
    p.mountpak_delegate = 0x934faa8;
    p.delegate_inst_off = 0;
    p.delegate_size_off = 8;
    p.handle_mount_pak = 0x176b900;
    return p;
}
