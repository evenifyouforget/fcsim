#!/usr/bin/env bash
# Install system depencences by running
cat requirements.system | xargs sudo apt install -y
# Additional dependencies for npm
sudo npm install --global prettier
# Many OS's forbid touching the global python, so we use venv
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install ruff prek scons pytest pytest-xdist
