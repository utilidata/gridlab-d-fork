#!/bin/bash
# Sets environment and runs GridLAB-D
# Developed for macOS - works to run GridLAB-D from a remote folder on a GLM in another folder

DIR="$(cd "$( dirname "${BASH_SOURCE[0]}")" > /dev/null && pwd)"
export GLPATH="${DIR}"
echo $GLPATH
GLMDIR=$(dirname "$@")
cd $GLMDIR
"${DIR}/gridlabd" "$@"
