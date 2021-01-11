#!/bin/bash
# ___________________________
#
# @File: build_zr.sh
# ___________________________
C_RED="\e[31m"
C_GREEN="\e[32m"
C_YELLOW="\e[33m"
C_BLUE="\e[34m"
C_RESET="\e[0m"
	

function check_dpkg()
{
    local  _pkgname=$1
    local  _pkginstalled=$(dpkg -s $_pkgname | sed -n '1 p' | awk '{print $2}')
    if [[ $_pkgname == $_pkginstalled ]]; then
      echo -e "$C_BLUE $_pkgname $C_RESET already installed [Ok]"
    else
      echo -e "$C_BLUE $_pkgname $C_RESET installing..."
      sudo apt install $_pkgname
    fi
}

function check_rpm()
{
    local  _pkgname=$1
    local  _pkginstalled=$(rpm -ql $_pkgname | sed -n '1 p' | awk '{print $2}')
    if [[ $_pkgname == $_pkginstalled ]]; then
      echo -e "$C_YELLOW $_pkgname $C_RESET already installed [Ok]"
    else
      echo -e "$C_YELLOW $_pkgname $C_RESET installing..."
      sudo yum install $_pkgname
    fi
}


# Are you from Debian or Redhat family ?
DISTRI_NAME=$(lsb_release -ar 2>/dev/null | sed -n '1 p' | awk '{print $3}') 
# Install package if necessary
if [[ ${DISTRI_NAME} == "Ubuntu" ]] || [[ ${DISTRI_NAME} == "Debian" ]]; then
  check_dpkg gprbuild
  SDL_VERSION=1.2
  check_dpkg libsdl$SDL_VERSION-dev
  check_dpkg libsdl-mixer$SDL_VERSION-dev
  check_dpkg libsdl-image$SDL_VERSION-dev
  check_dpkg libsdl-gfx$SDL_VERSION-dev
  check_dpkg libsdl-net$SDL_VERSION-dev
  check_dpkg libsdl-ttf2.0-dev
  check_dpkg libsdl-mixer$SDL_VERSION
  check_dpkg libsdl-image$SDL_VERSION
  check_dpkg libsdl-net$SDL_VERSION
  # check if sound card is present
  PLAYWAVE=$(play snd/run.wav 2>/dev/null )
  sudo modprobe snd-hda-intel 

elif [[ ${DISTRI_NAME} == "Redhat" ]] || [[ ${DISTRI_NAME} == "CentOS" ]]; then
  check_rpm gprbuild
else
  echo -e "Unknown ${DISTRI_NAME} but can continue!"
fi

# Build processing...
mkdir -p ./ZorgonsRevenge/obj
mkdir -p ./liboric/obj

gprbuild -P ./ZorgonsRevenge/ZorgonsRevenge.gpr 2>&1 | tee log.txt
./ZorgonsRevenge/zorgon -em

