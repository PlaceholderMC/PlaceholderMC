#!/usr/bin/env bash

set -e

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &>/dev/null && pwd)"
cd "${SCRIPT_DIR}"

function can_exec {
  if [[ -z "$1" ]]; then
    echo "Error: can_exec requires 1 argument."
    return 1
  fi

  if local full_path="$(command -v "$1" 2>/dev/null)"; then
    if [[ ! -z "${full_path}" ]] && [[ -x "${full_path}" ]]; then
      return 0
    fi
  fi

  return 1
}

function show_help {
  echo "Flatpak Builder for Prism Launcher."
  echo
  echo "Syntax: build-flatpak.sh [options...]"
  echo
  echo "Options:"
  echo "-i     Install the application after building."
  echo "-e     Export to a distributable \".flatpak\" bundle."
  echo "-r     Run the application after installation (requires -i)."
  echo "-u     Use the current user's Flatpak environment instead of System."
  echo "-h     Show the help."
  echo
}

_OPT_FLATPAK_USER="false"
_OPT_INSTALL="false"
_OPT_EXPORT="false"
_OPT_RUN="false"

while getopts ":huier" option; do
  case $option in
  h)
    show_help
    exit
    ;;
  u)
    _OPT_FLATPAK_USER="true"
    ;;
  i)
    _OPT_INSTALL="true"
    ;;
  e)
    _OPT_EXPORT="true"
    ;;
  r)
    _OPT_RUN="true"
    ;;
  \?)
    echo "Error: Invalid option. Use -h to show the usage instructions."
    exit 1
    ;;
  esac
done

if [[ "${_OPT_RUN}" == "true" ]] && [[ "${_OPT_INSTALL}" != "true" ]]; then
  echo "Error: The -r option also requires the -i option. Use -h to show the usage instructions."
  exit 1
fi

BUILD_FLATPAK_ENVIRONMENT="system"
BUILD_SUDO_CMD=("sudo")
if [[ "${_OPT_FLATPAK_USER}" == "true" ]]; then
  BUILD_FLATPAK_ENVIRONMENT="user"
  BUILD_SUDO_CMD=()
fi

echo "Flatpak Build Options:"
echo "- Flatpak Environment: ${BUILD_FLATPAK_ENVIRONMENT}"
echo "- Install: ${_OPT_INSTALL}"
echo "- Export: ${_OPT_EXPORT}"
echo "- Run: ${_OPT_RUN}"
echo

BUILD_APP_ID="org.prismlauncher.PrismLauncher"
BUILD_MANIFEST="${BUILD_APP_ID}.yml"
BUILD_DIR="flatbuild/build"
BUILD_OSTREE_REPO_DIR="flatbuild/repo"

BUILD_CMD=("flatpak-builder")
if ! can_exec "${BUILD_CMD[0]}"; then
  BUILD_CMD=("flatpak" "run" "org.flatpak.Builder")
fi

declare -a BUILD_EXTRA_ARGS=()
#BUILD_EXTRA_ARGS+=("--example-dynamically-appended-arg")

set -x

# Build the binaries, but don't create OSTree repo unless we need it (that's slow).
"${BUILD_CMD[@]}" --verbose \
  "--${BUILD_FLATPAK_ENVIRONMENT}" \
  --install-deps-from=flathub \
  --force-clean --ccache \
  "${BUILD_EXTRA_ARGS[@]}" \
  "${BUILD_DIR}" \
  "${BUILD_MANIFEST}"

# Export to a local OSTree repository directory, but only if necessary.
if [[ "${_OPT_INSTALL}" == "true" ]] || [[ "${_OPT_EXPORT}" == "true" ]]; then
  flatpak build-export --verbose \
    "${BUILD_OSTREE_REPO_DIR}" \
    "${BUILD_DIR}"
fi

# Install from the local repository directory.
# NOTE: Flatpak ONLY treats repos beginning with "/" or "./" as local.
# NOTE: System installations REQUIRE sudo when using a temporary (local) remote,
# which protects Flatpak against randomly adding hostile packages to System.
if [[ "${_OPT_INSTALL}" == "true" ]]; then
  "${BUILD_SUDO_CMD[@]}" flatpak install --verbose \
    "--${BUILD_FLATPAK_ENVIRONMENT}" \
    -y --noninteractive --reinstall \
    "./${BUILD_OSTREE_REPO_DIR}" \
    "${BUILD_APP_ID}"
fi

# Export to distributable ".flatpak" bundle (this takes a very long time).
if [[ "${_OPT_EXPORT}" == "true" ]]; then
  echo "Exporting \".flatpak\" bundle. Please be patient, this takes a long time..."
  flatpak build-bundle --verbose \
    --runtime-repo=https://flathub.org/repo/flathub.flatpakrepo \
    "${BUILD_OSTREE_REPO_DIR}" \
    "${BUILD_APP_ID}.flatpak" \
    "${BUILD_APP_ID}"
fi

echo "Build complete."

# Run the locally installed application.
if [[ "${_OPT_RUN}" == "true" ]]; then
  flatpak run "--${BUILD_FLATPAK_ENVIRONMENT}" "${BUILD_APP_ID}"
fi
