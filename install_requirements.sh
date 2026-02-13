# Install system depencences by running
cat requirements.system | xargs sudo apt install -y
# Additional dependencies for specific package managers
sudo npm install --global prettier
python3 -m pip install ruff pre-commit
