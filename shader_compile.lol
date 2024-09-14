HAI 1.2
CAN HAS STDIO?

I HAS A directory
I HAS A shader_file
I HAS A compile_cmd

HOW IZ I compile_shader YR file
    compile_cmd R SMOOSH "glslc " AN file AN " -o " AN file AN ".spv" MKAY
    VISIBLE SMOOSH "EXECUTIN: " AN compile_cmd MKAY
    BTW In a real system, we would execute the command here
    BTW But LOLCODE can't actually do this, so we're just printing it
IF U SAY SO

VISIBLE "WHATS UR SHADER DIRECTORY?"
GIMMEH directory

VISIBLE SMOOSH "COMPILIN SHADERS IN: " AN directory MKAY

VISIBLE "ENTER SHADER FILENAMES (WITH EXTENSION) ONE BY ONE. TYPE 'DONE' WHEN FINISHED."

IM IN YR loop
    VISIBLE "ENTER SHADER FILENAME:"
    GIMMEH shader_file
    BOTH SAEM shader_file AN "DONE", O RLY?
        YA RLY
            VISIBLE "COMPILASHUN COMPLETE!"
            GTFO
        NO WAI
            I HAS A file_path
            file_path R SMOOSH directory AN "/" AN shader_file MKAY
            I IZ compile_shader YR file_path MKAY
    OIC
IM OUTTA YR loop

KTHXBYE
