{
  stdenv,
  pkgs,
  fetchFromGitHub,
  autoreconfHook,
  lib,
}:
stdenv.mkDerivation rec {
  pname = "xmrig-switch";
  version = lib.fileContents ./version.txt;

  src = builtins.path {
    name = "xmrig-switch-src";
    path = ./.;
  };

  buildInputs = with pkgs; [
    systemdLibs
    libudev-zero
    pkg-config
  ];

  nativeBuildInputs = [autoreconfHook];

  meta = {
    description = "XMRig Switch controller module";
    mainProgram = "xmrig-switch";
  };
}
