{
  pkgs,
  stdenv,
  autoreconfHook,
  fetchFromGitHub
}:
stdenv.mkDerivation rec {
	pname = "xmrig-switch";
	version = "1";

	src = fetchFromGitHub {
		owner = "caeklol";
		repo = "xmrig-switch";
		rev = "master";
		hash = "sha256-0paYJMx9Yy5bSnDKntUVgxIBlkUBUfvvez9rNGl5aMk=";
	};

	buildInputs = with pkgs; [
        systemdLibs
        pkg-config
    ];

	nativeBuildInputs = [ autoreconfHook ];
}
