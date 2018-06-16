#!/bin/bash
set -e
set -x
rm -rf files var metadata export build

BRANCH=${BRANCH:-master}
GIT_CLONE_BRANCH=${GIT_CLONE_BRANCH:-HEAD}
RUN_TESTS=${RUN_TESTS:-false}
PROJECT=${PROJECT:-com.endlessm.EknServices3}

sed \
  -e "s|@BRANCH@|${BRANCH}|g" \
  -e "s|@GIT_CLONE_BRANCH@|${GIT_CLONE_BRANCH}|g" \
  -e "s|\"@RUN_TESTS@\"|${RUN_TESTS}|g" \
  com.endlessm.EknServices.json.in \
  > com.endlessm.EknServices.json

flatpak-builder build -vv --repo=repo com.endlessm.EknServices.json
flatpak build-update-repo repo
flatpak build-bundle repo ${PROJECT}.flatpak ${PROJECT} ${BRANCH}
flatpak build-bundle --runtime repo ${PROJECT}.Extension.flatpak ${PROJECT}.Extension ${BRANCH}
