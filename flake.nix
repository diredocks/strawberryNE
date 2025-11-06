{
  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs, ... }:
    let
      system = "x86_64-linux";
      pkgs = import nixpkgs {
        inherit system;
        config.allowUnfree = true;
      };
    in {
      packages.${system}.default = pkgs.stdenv.mkDerivation (finalAttrs: {
        pname = "strawberry";
        version = "dev";

        src = ./.;

        buildInputs = with pkgs; [
          alsa-lib
          boost
          chromaprint
          fftw
          gnutls
          kdsingleapplication
          libxdmcp
          libcdio
          libebur128
          libidn2
          libmtp
          xorg.libpthreadstubs
          libtasn1
          qt6.qtbase
          sqlite
          taglib
          sparsehash
          rapidjson
          libpulseaudio
          libselinux
          libsepol
          p11-kit
          glib-networking
          gst_all_1.gst-libav
          gst_all_1.gst-plugins-bad
          gst_all_1.gst-plugins-base
          gst_all_1.gst-plugins-good
          gst_all_1.gst-plugins-ugly
          gst_all_1.gst-plugins-rs
          gst_all_1.gstreamer
        ];

        nativeBuildInputs = with pkgs; [
          cmake
          ninja
          pkg-config
          qt6.qttools
          qt6.wrapQtAppsHook
          util-linux
        ];

        cmakeFlags = [ (pkgs.lib.cmakeBool "ENABLE_GPOD" false) ];

        # checkInputs = [ pkgs.gtest ];
        # checkTarget = "strawberry_tests";
        # preCheck = ''
        #   export QT_QPA_PLATFORM=offscreen
        # '';
        # doCheck = false;
      });

      devShells.${system}.default = pkgs.mkShell {
        name = "strawberry-dev";
        inputsFrom = [ self.packages.${system}.default ];

        nativeBuildInputs = with pkgs; [
          gcc
          cmake
          ninja
          pkg-config
          qt6.qttools
          qt6.wrapQtAppsHook
          clang-tools
        ];

        buildInputs = with pkgs; [
          qt6.qtbase
          qt6.qthttpserver
          qt6.qtwebsockets
          boost
          sqlite
          taglib
          alsa-lib
          fftw
          chromaprint
          gst_all_1.gstreamer
          gst_all_1.gst-plugins-base
          gst_all_1.gst-plugins-good
          gst_all_1.gst-plugins-bad
          gst_all_1.gst-plugins-ugly
          openssl
        ];
      };
    };
}
