{
  stdenv,
  fetchFromGithub,
  autoreconfHook
}:
stdenv.mkDerivation rec {
	pname = "xmrig-switch";
	version = "1";

	src = fetchFromGithub {
		owner = "caeklol";
		repo = "xmrig-switch";
	};

	buildInputs = with pkgs; [
        systemdLibs
        pkg-config
    ];

	nativeBuildInputs = [ autoreconfHook ];
}
