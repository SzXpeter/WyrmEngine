add_custom_command(
    OUTPUT shader.spv
    COMMAND python compile_slang.py
    DEPENDS shader.slang
)
