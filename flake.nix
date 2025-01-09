{
  description = "A custom launcher for Minecraft that allows you to easily manage multiple installations of Minecraft at once (Fork of MultiMC)";

  nixConfig = {
    extra-substituters = [ "https://prismlauncher.cachix.org" ];
    extra-trusted-public-keys = [
      "prismlauncher.cachix.org-1:9/n/FGyABA2jLUVfY+DEp4hKds/rwO+SCOtbOkDzd+c="
    ];
  };

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";

    libnbtplusplus = {
      url = "github:PrismLauncher/libnbtplusplus";
      flake = false;
    };

    qt-qrcodegenerator = {
      url = "github:nayuki/QR-Code-generator";
      flake = false;
    };

    nix-filter.url = "github:numtide/nix-filter";

    /*
      Inputs below this are optional and can be removed

      ```
      {
      inputs.prismlauncher = {
        url = "github:PrismLauncher/PrismLauncher";
        inputs = {
         	    flake-compat.follows = "";
        };
      };
      }
      ```
    */

    flake-compat = {
      url = "github:edolstra/flake-compat";
      flake = false;
    };
  };

  outputs =
    {
      self,
      nixpkgs,
      libnbtplusplus,
      qt-qrcodegenerator,
      nix-filter,
      ...
    }:
    let
      inherit (nixpkgs) lib;

      # While we only officially support aarch and x86_64 on Linux and MacOS,
      # we expose a reasonable amount of other systems for users who want to
      # build for most exotic platforms
      systems = lib.systems.flakeExposed;

      forAllSystems = lib.genAttrs systems;
      nixpkgsFor = forAllSystems (system: nixpkgs.legacyPackages.${system});
    in
    {
      checks = forAllSystems (
        system:
        let
          checks' = nixpkgsFor.${system}.callPackage ./nix/checks.nix { inherit self; };
        in
        lib.filterAttrs (_: lib.isDerivation) checks'
      );

      devShells = forAllSystems (
        system:
        let
          pkgs = nixpkgsFor.${system};
        in
        {
          default = pkgs.mkShell {
            inputsFrom = [ self.packages.${system}.prismlauncher-unwrapped ];
            buildInputs = with pkgs; [
              ccache
              ninja
            ];
          };
        }
      );

      formatter = forAllSystems (system: nixpkgsFor.${system}.nixfmt-rfc-style);

      overlays.default = final: prev: {
        prismlauncher-unwrapped = prev.callPackage ./nix/unwrapped.nix {
          inherit
            libnbtplusplus
            qt-qrcodegenerator
            nix-filter
            self
            ;
        };

        prismlauncher = final.callPackage ./nix/wrapper.nix { };
      };

      packages = forAllSystems (
        system:
        let
          pkgs = nixpkgsFor.${system};

          # Build a scope from our overlay
          prismPackages = lib.makeScope pkgs.newScope (final: self.overlays.default final pkgs);

          # Grab our packages from it and set the default
          packages = {
            inherit (prismPackages) prismlauncher-unwrapped prismlauncher;
            default = prismPackages.prismlauncher;
          };
        in
        # Only output them if they're available on the current system
        lib.filterAttrs (_: lib.meta.availableOn pkgs.stdenv.hostPlatform) packages
      );

      # We put these under legacyPackages as they are meant for CI, not end user consumption
      legacyPackages = forAllSystems (
        system:
        let
          prismPackages = self.packages.${system};
          legacyPackages = self.legacyPackages.${system};
        in
        {
          prismlauncher-debug = prismPackages.prismlauncher.override {
            prismlauncher-unwrapped = legacyPackages.prismlauncher-unwrapped-debug;
          };

          prismlauncher-unwrapped-debug = prismPackages.prismlauncher-unwrapped.overrideAttrs {
            cmakeBuildType = "Debug";
            dontStrip = true;
          };
        }
      );
    };
}
