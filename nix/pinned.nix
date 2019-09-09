# See https://nixos.wiki/wiki/FAQ/Pinning_Nixpkgs for more information on pinning
builtins.fetchTarball {
  # Descriptive name to make the store path easier to identify
  name = "nixpkgs-unstable-2019-09-10";
  # Commit hash for nixpkgs-unstable as of 2019-09-10
  url = https://github.com/NixOS/nixpkgs-channels/archive/862e91dc6bc3722438a476d65e710f89ec66b379.tar.gz;
  # Hash obtained using `nix-prefetch-url --unpack <url>`
  sha256 = "1vj5cdy076yaa6hb6j3x8g1az10c5y0rivawjys6v7bggy43335i";
}
