set_project("OmegaWare-Localization")
set_version("0.1.0")

add_rules("mode.debug", "mode.release")

local function LlvmTool(strName)
    local strTool = path.join(os.getenv("ProgramFiles") or "", "LLVM", "bin", strName .. ".exe")
    return os.isfile(strTool) and strTool or strName
end
set_toolchains("clang-cl")
set_toolset("ld", LlvmTool("lld-link"))
set_toolset("sh", LlvmTool("lld-link"))
set_toolset("ar", LlvmTool("llvm-ar"))

if is_mode("debug") then
    set_symbols("debug")
end

set_languages("c++latest")
set_warnings("all")
set_encodings("utf-8")
set_runtimes(is_mode("debug") and "MTd" or "MT")

includes("Source/OmegaLocalization/xmake.lua")

target("Tests")
    set_kind("binary")
    set_targetdir(path.join("Build", is_mode("debug") and "Debug" or "Release", "$(arch)"))
    set_filename("OmegaLocalization-Tests.exe")
    add_deps("OmegaLocalization")
    add_files("Source/Tests/*.cpp")
    add_includedirs(".")
target_end()
