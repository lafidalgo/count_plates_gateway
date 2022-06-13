#!/bin/bash
echo "Atualizando Repositorios..."
if ! sudo apt-get update
then
echo "Nao foi possivel atualizar os repositorios"
exit 1
fi
echo "Iniciando download das ferramentas necessarias para o IDF"
if ! sudo apt-get install git wget flex bison gperf python python-pip python-setuptools cmake ninja-build ccache libffi-dev libssl-dev dfu-util -y
then
echo "Falha no download das ferramentas"
exit 1
fi
echo "Baixando o python 3 e tornando como o interpretador default para o ambiente"
if ! sudo apt-get install python3 python3-pip python3-setuptools -y
then
echo "Falha ao baixar python 3"
fi
if ! sudo update-alternatives --install /usr/bin/python python /usr/bin/python3 10
then
echo "Falha ao tornar python3 o interpretador padrao"
exit 1
fi
echo "Clonando o repositorio IDF"
if ! (mkdir -p ~/esp && cd ~/esp && git clone --recursive https://github.com/espressif/esp-idf.git)
then
echo "Falha ao clonar o repositorio da idf"
exit 1
fi
echo "Executando instalacao do IDF"
if ! (cd ~/esp/esp-idf && ./install.sh)
then
echo "Falha ao instalar idf"
exit 1
fi