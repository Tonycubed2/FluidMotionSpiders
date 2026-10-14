set_xmakever("3.0.0")
set_project("FluidMotionSpiders")
set_version("1.0.0")
set_arch("x64")
set_languages("c++23")
set_warnings("allextra")
add_rules("mode.debug", "mode.releasedbg")
set_defaultmode("releasedbg")

set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", true)
set_config("rex_ini", true)
set_config("skse_xbyak", true)

includes("lib/commonlibsse-ng")

add_requires("xbyak")

target("FluidMotionSpiders", function()
    add_deps("commonlibsse-ng")
    add_packages("xbyak")
    add_rules("commonlibsse-ng.plugin", {
        name = "FluidMotionSpiders",
        author = "megakuutti, Tony Torres",
        description = "Fluid Motion spider IK legs and wall crawling"
    })

    add_files("src/**.cpp")
    add_headerfiles("src/**.h")
    add_includedirs("src")
    set_pcxxheader("src/PCH.h")

    add_installfiles("dist/(SKSE/Plugins/*.ini)")
end)
