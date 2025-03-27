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

pub fn build(b: *std.Build) !void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const disable_bounds_checks = b.option(bool, "disable-bounds-checks", "Disables bounds checking for user created types") orelse true;
    const enable_tracy = b.option(bool, "enable-tracy", "Build with support for profiling with the tracy profiler.") orelse false;
    _ = disable_bounds_checks;

    var flags = std.ArrayList([]const u8).init(b.allocator);
    try flags.appendSlice(&.{ "-std=c++23", "-Wall", "-Wextra", "-Wunused-result", "-Werror", "-Wno-missing-field-initializers", "-Wno-unused-function" });

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

    // vma
    {
        const vma_dep = b.dependency("vulkan_memory_allocator", .{});
        exe.addIncludePath(vma_dep.path("include"));
        exe.addIncludePath(vma_dep.path("src"));
        exe.addCSourceFile(.{
            .file = vma_dep.path("src/VmaUsage.cpp"),
        });
    }

    // Tracy
    {
        const tracy_dep = b.dependency("tracy", .{});
        exe.addIncludePath(tracy_dep.path("public"));
        if (enable_tracy) {
            try flags.append("-DTRACY_ENABLE");
            exe.addCSourceFile(.{
                .file = tracy_dep.path("public/TracyClient.cpp"),
                .flags = &.{"-DTRACY_ENABLE"},
            });

            switch (target.result.os.tag) {
                .windows => {
                    exe.linkSystemLibrary("ws2_32");
                    exe.linkSystemLibrary("dbghelp");
                },
                else => @panic("TODO"),
            }
        }
    }

    exe.linkLibCpp();
    exe.addCSourceFiles(.{
        .files = &win32_sources,
        .flags = flags.items,
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
