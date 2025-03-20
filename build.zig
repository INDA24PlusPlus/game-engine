const std = @import("std");

const engine_source_files = [_][]const u8{
    "src/engine/renderer.cpp",
};

const game_source_files = [_][]const u8{
    "src/game/game.cpp",
};

const win32_sources = [_][]const u8{
    "src/engine/win32_main.cpp",
    "src/engine/win32_platform.cpp",
} ++ engine_source_files ++ game_source_files;

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const disable_bounds_checks = b.option(bool, "disable-bounds-checks", "Disables bounds checking for user created types") orelse true;
    _ = disable_bounds_checks;

    const exe = b.addExecutable(.{
        .name = "vulkan_renderer",
        .target = target,
        .optimize = optimize,
    });
    exe.addIncludePath(b.path("src"));

    // Vulkan headers (vulkan.h, etc)
    {
        const vulkan_headers_dep = b.dependency("vulkan_headers", .{});
        exe.addIncludePath(vulkan_headers_dep.path("include"));
    }

    // VOLK
    {
        const vulkan_os_define = switch (target.result.os.tag) {
            .windows => "VK_USE_PLATFORM_WIN32_KHR",
            else => @panic("Unsupported OS"),
        };

        const volk_dep = b.dependency("volk", .{});
        exe.addCSourceFile(.{
            .file = volk_dep.path("volk.c"),
            .flags = &.{ "-D", vulkan_os_define },
        });
        exe.addIncludePath(volk_dep.path(""));
    }

    // GLM
    {
        const glm_dep = b.dependency("glm", .{});
        exe.addIncludePath(glm_dep.path(""));
    }

    //imGUI
    {
        const imgui_dep = b.dependency("imgui", .{});
        const imgui_source_files = [_][]const u8{
            "imgui.cpp",
            "imgui_demo.cpp",
            "imgui_draw.cpp",
            "imgui_tables.cpp",
            "imgui_widgets.cpp",
            "backends/imgui_impl_vulkan.cpp",
        };
        exe.addIncludePath(imgui_dep.path(""));
        exe.addIncludePath(imgui_dep.path("backends"));
        for (&imgui_source_files) |src| {
            exe.addCSourceFile(.{ .file = imgui_dep.path(src), .flags = &.{"-DIMGUI_IMPL_VULKAN_USE_VOLK"} });
        }
    }

    exe.linkLibCpp();
    exe.addCSourceFiles(.{
        .files = &win32_sources,
        .flags = &.{ "-std=c++23", "-Wall", "-Wextra", "-Wunused-result", "-Werror", "-Wno-missing-field-initializers", "-Wno-unused-function" },
    });

    b.installArtifact(exe);

    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);
}
