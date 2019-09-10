{ pkgs ? import (import nix/pinned.nix) {} }:
let
  stdenv = pkgs.stdenv;
  caf = pkgs.caf.overrideAttrs (_: {
    src = pkgs.fetchFromGitHub {
      owner = "actor-framework";
      repo = "actor-framework";
      rev = "5dff9fbccc5ff954f0b26f1124c9985e55642d79";
      sha256 = "1kccid8q14l541kaxij8a3z8qdmf2xkkxjlcax8m229qr65xy63w";
    };
  });

  python = pkgs.python3Packages.python.withPackages( ps: with ps; [
    coloredlogs
    pyyaml
    schema
  ]);
in
stdenv.mkDerivation {
  pname = "vast";
  version = stdenv.lib.fileContents ./VERSION;

  src = pkgs.nix-gitignore.gitignoreSource [] ./.;

  nativeBuildInputs = [ pkgs.cmake ];
  buildInputs = [ caf pkgs.libpcap ];

  cmakeFlags = [
    "-DCAF_ROOT_DIR=${caf}"
    "-DNO_AUTO_LIBCPP=ON"
    "-DENABLE_ZEEK_TO_VAST=OFF"
  ];

  preBuild = ''
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/lib
  '';

  doCheck = true;
  checkTarget = "test";
  preCheck = ''
    export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$PWD/lib
  '';

  doInstallCheck = true;
  installCheckInputs = [ python ];
  installCheckPhase = ''
    integration/integration.py --app ${placeholder "out"}/bin/vast
  '';

  meta = with pkgs.lib; {
    description = "Visibility Across Space and Time";
    homepage = http://vast.io/;
    license = licenses.bsd3;
    platforms = platforms.unix;
    maintainers = with maintainers; [ tobim ];
  };
}
