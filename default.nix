{ pkgsi686Linux, texinfo }:
pkgsi686Linux.stdenv.mkDerivation rec {
  pname = "klisp";
  version = "git";

  buildInputs = with pkgsi686Linux; [ libffi ];
  nativeBuildInputs = [ texinfo ];

  src = builtins.path { path = ./.; name = "klisp"; };

  NIX_CFLAGS_COMPILE = "-fcommon";

  buildPhase = ''
    runHook preBuild

    pushd src
    make clean
    make ''${enableParallelBuilding:+-j''${NIX_BUILD_CORES}} posix USE_LIBFFI=1 CC="$CC" AR="$AR rcu" RANLIB="$RANLIB"
    popd

    pushd doc/src
    make clean
    make ''${enableParallelBuilding:+-j''${NIX_BUILD_CORES}} info html
    popd

    runHook postBuild
  '';

  installPhase = ''
      runHook preInstall

    	mkdir -p "$out"/bin/
      cp src/klisp "$out"/bin/

    	mkdir -p "$out"/share/man/man1/
      cp doc/klisp.1 "$out"/share/man/man1/

    	mkdir -p "$out"/share/info/
      cp doc/klisp.info "$out"/share/info/

    	mkdir -p "$out"/share/doc/klisp/
      cp -r doc/html/. "$out"/share/doc/klisp/

    	mkdir -p "$out"/share/klisp/
      cp -r src/examples src/tests README COPYRIGHT CHANGES TODO "$out"/share/klisp/

      runHook postInstall
  '';

  enableParallelBuilding = true;
}
