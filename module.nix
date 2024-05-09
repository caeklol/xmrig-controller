self: {
  xmrig-switch,
  config,
  pkgs,
  lib,
  ...
}:
{
  options = {
    programs.xmrig-switch = {
      enable = lib.mkEnableOption "xmrig-switch";
    };
  };

  config = {
    systemd.services.xmrig-switch = {
      wantedBy = ["multi-user.target"];
      after = ["network.target"];
      description = "XMRig Controller using input from Serial";
      serviceConfig = {
        ExecStart = "${lib.getExe self.packages.${pkgs.stdenv.hostPlatform.system}.xmrig-switch}";
        DynamicUser = false;
        StandardError = "journal";
        StandardOutput = "journal";
        StandardInput = null;
      };
    };
  };
}
