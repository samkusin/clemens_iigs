#!/bin/bash


GPG_KEY=Put Your GPG Key Here


function clearWorkDirs() {
  [[ -d .flatpak-builder ]] && rm -rf .flatpak-builder
  [[ -d flatpak-build ]]    && rm -rf flatpak-build
  [[ -d flatpak-repo ]]     && rm -rf flatpak-repo
}


flatpak install flathub org.freedesktop.Platform//22.08 org.freedesktop.Sdk//22.08

clearWorkDirs
[[ -f clemens.flatpak ]] && rm -f clemens.flatpak

flatpak-builder --user --gpg-sign=${GPG_KEY} -v --keep-build-dirs --repo=flatpak-repo --install flatpak-build com.samirsinha.Clemens.yaml
flatpak build-bundle flatpak-repo clemens.flatpak com.samirsinha.Clemens --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo

[[ -f clemens.flatpak ]] && clearWorkDirs
