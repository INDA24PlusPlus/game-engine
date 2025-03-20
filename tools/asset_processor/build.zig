const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "asset_processor",
        .target = target,
        .optimize = optimize,
    });
    exe.linkLibCpp();
    exe.addCSourceFiles(.{
        .files = &.{"src/main.cpp"},
        .flags = &.{ "-std=c++23", "-Wall", "-Wextra", "-Wunused-result", "-Werror", "-Wno-missing-field-initializers", "-Wno-unused-function" },
    });

    const tiny_gltf_dep = b.dependency("tiny_gltf", .{});
    exe.addIncludePath(tiny_gltf_dep.path(""));
    exe.addCSourceFile(.{
        .file = tiny_gltf_dep.path("tiny_gltf.cc"),
        .flags = &.{ "-DTINYGLTF_NOEXCEPTION", "-DJSON_NOEXCEPTION" },
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
