{
  description = "Eris Dev Flake";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
  let
    system = "x86_64-linux";
    pkgs = nixpkgs.legacyPackages.${system};
  in
  {
    devShells.${system}.default =
      pkgs.mkShell
        {
          nativeBuildInputs = [
            pkgs.pkg-config
            pkgs.clang-tools
            pkgs.llvmPackages_19.lldb
            pkgs.llvmPackages_19.libllvm
            pkgs.llvmPackages_19.libcxx
            pkgs.llvmPackages_19.clang
            pkgs.xorg.xrandr
            pkgs.xorg.libX11
          ];
          buildInputs = [
            pkgs.pkg-config
            pkgs.udev 
            pkgs.alsa-lib 
            pkgs.vulkan-loader
            pkgs.libxkbcommon
            pkgs.wayland
            pkgs.xorg.libX11
            pkgs.xorg.libXi
            pkgs.xorg.libXcursor
            pkgs.xorg.libXrandr
            pkgs.xorg.libXinerama
            pkgs.llvmPackages_19.lldb
            pkgs.llvmPackages_19.libllvm
            pkgs.llvmPackages_19.libcxx
            pkgs.llvmPackages_19.clang
            pkgs.libGL
            pkgs.gcc
            pkgs.SDL2
            pkgs.cmake
          ];
          LD_LIBRARY_PATH = "${pkgs.lib.makeLibraryPath [
            pkgs.udev 
            pkgs.alsa-lib 
            pkgs.vulkan-loader
            pkgs.libxkbcommon
            pkgs.wayland
            pkgs.xorg.libX11
            pkgs.xorg.libXi
            pkgs.xorg.libXcursor
            pkgs.xorg.xrandr
            pkgs.libGL
            pkgs.gcc
            pkgs.SDL2
          ]}";
        };

  };
}
