{ pkgs ? import <nixpkgs> { } }:
let
  original = pkgs.callPackage ./. { };
in
original.overrideAttrs (old: {
  nativeBuildInputs = old.nativeBuildInputs ++ (with pkgs; [ gdb ]);
})
