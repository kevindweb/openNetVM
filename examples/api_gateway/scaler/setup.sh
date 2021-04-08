#!/bin/bash

echo "Installing packages"
sudo apt-get update
sudo apt-get upgrade
sudo apt-get install -y \
    apt-transport-https \
    ca-certificates \
    curl \
    gnupg \
    lsb-release

echo "Getting docker"
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /usr/share/keyrings/docker-archive-keyring.gpg
echo \
  "deb [arch=amd64 signed-by=/usr/share/keyrings/docker-archive-keyring.gpg] https://download.docker.com/linux/ubuntu \
  $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null

sudo apt-get update
sudo apt-get install docker-ce docker-ce-cli containerd.io

echo "Allow docker without sudo"
sudo usermod -aG docker $USER

echo "Setup docker stack"
docker swarm init

echo "Remember to run dockerfile in examples/api_gateway/scaler to set up container NF"