{
  description = "Build environment for Haiku on the SGI Indy";

  # The same nixpkgs release as /mnt/europa/project, so both share store paths.
  inputs.nixpkgs.url = "github:NixOS/nixpkgs/nixos-26.05";

  outputs = { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};

      # Upstream Haiku builds its host tools with GCC 13.
      hostStdenv = pkgs.gcc13Stdenv;
      hostGcc = pkgs.gcc13;

      # Haiku's jam. Jamrules rejects every other jam variant.
      haikuJam = hostStdenv.mkDerivation {
        pname = "haiku-jam";
        version = "2.5-haiku-20211029";

        src = pkgs.fetchFromGitHub {
          owner = "haiku";
          repo = "buildtools";
          rev = "33d46bf5c300bd34cd07c596431e4545c754ac03";
          sparseCheckout = [ "jam" ];
          hash = "sha256-pyDOeUmwn3yu3jWBVqX9bO0N4/ccqajOTlNLRxBhqBs=";
        };
        sourceRoot = "source/jam";

        nativeBuildInputs = [ pkgs.bison ];
        hardeningDisable = [ "all" ];
        enableParallelBuilding = false;

        buildPhase = ''
          make
        '';
        installPhase = ''
          mkdir -p $out/bin
          ./jam0 -sBINDIR=$out/bin install
        '';
      };

      # GCC's build prerequisites in one prefix, for --with-gmp and friends.
      hostPrereqs = pkgs.symlinkJoin {
        name = "haiku-indy-host-prereqs";
        paths = with pkgs; [
          gmp.dev gmp.out
          mpfr.dev mpfr.out
          libmpc
          isl
        ];
      };
    in {
      packages.${system}.jam = haikuJam;

      devShells.${system}.default = pkgs.mkShell.override { stdenv = hostStdenv; } {
        name = "haiku-indy-dev";

        # Hardening flags turn warnings in Haiku's host tools and in the GCC
        # sources into errors.
        hardeningDisable = [ "all" ];

        packages = with pkgs; [
          haikuJam
          hostGcc
          binutils
          gnumake
          bison
          flex
          gawk
          gperf
          m4
          texinfo
          autoconf
          automake
          # for haikuporter's host side of the bootstrap ports
          libtool
          cmake
          pkg-config
          perl
          python3
          git
          wget
          curl
          cacert
          xz
          bzip2
          gzip
          unzip
          patch
          diffutils
          file
          netcat
          nasm
          mtools
          xorriso
        ];

        # Headers and libraries Haiku's host tools link against.
        buildInputs = with pkgs; [
          zlib
          zstd
        ];

        GMP_PREFIX = "${hostPrereqs}";
        MPFR_PREFIX = "${hostPrereqs}";
        MPC_PREFIX = "${hostPrereqs}";
        ISL_PREFIX = "${hostPrereqs}";
        SSL_CERT_FILE = "${pkgs.cacert}/etc/ssl/certs/ca-bundle.crt";

        shellHook = ''
          export HAIKU_TOOLCHAIN="''${HAIKU_TOOLCHAIN:-/mnt/europa/haiku-toolchain}"
          export PATH="$HAIKU_TOOLCHAIN/cross-tools-mips/bin:$PATH"
          echo "haiku-indy-dev: $(jam -v 2>&1 | head -1)"
        '';
      };
    };
}
