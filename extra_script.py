Import("env")

# Patch the platform's ElfToBin builder before it's used
# This needs to run as a pre-script

def patch_elf2image(env):
    # Find and replace the ElfToBin builder's action
    for builder_name, builder in env['BUILDERS'].items():
        if 'ElfToBin' in builder_name or 'elf2image' in str(builder).lower():
            print(f"Found builder: {builder_name}")
            # Replace the action
            env['BUILDERS'][builder_name] = env.Builder(
                action='"$PYTHONEXE" "$OBJCOPY" --chip esp32s3 elf2image --flash_mode dio --flash_freq 80m --flash_size 16MB --elf-sha256-offset 0xb0 -o $TARGET $SOURCES',
                suffix='.bin',
                src_suffix='.elf'
            )
            print(f"Patched {builder_name}")

# Run immediately
patch_elf2image(env)

# Also try to patch the default action for firmware.bin
env.AddPostAction("firmware.elf", '"$PYTHONEXE" "$OBJCOPY" --chip esp32s3 elf2image --flash_mode dio --flash_freq 80m --flash_size 16MB --elf-sha256-offset 0xb0 -o ${TARGET.base}.bin $SOURCE')

print("Post-action added for firmware.elf -> firmware.bin")