## Debug Travis CI with Docker

Host PC: Ubuntu 16.04 LTS with "su root"

Reference: https://docs.travis-ci.com/user/common-build-problems/#Running-a-Container-Based-Docker-Image-Locally

* Install Docker: apt-get install docker
* Download Travis CI Docker image(refer to https://hub.docker.com/u/travisci/): docker pull travisci/ci-garnet:packer-1490989530
* Start Docket service: service docker start
* List existing images: docker images
* Don't use this vritual net interface as we only need use --net=host: ifconfig docker0 down
* Run Travis CI image in Docker: docker run --name travis-debug-001 --rm -v /mnt:/mnt/  --net=host -dit travisci/ci-garnet:packer-1490989530 /sbin/init
* Switch to shell: docker exec -it travis-debug-001 bash -l
* Setup network, e.g. proxy....to install dependance packages
* Save changed image to new name: docker commit travis-debug-001 travis-debug-002

## Commits of headers & libs
* linux: https://github.com/android-ia/device-androidia-kernel.git "master" branch 9d2ec87a7f431fd699dc4b52c02fd421d961329d
* libdrm: https://github.com/android-ia/external-libdrm.git "master" branch 82a4b6756572a9d708f174708615bddb2f472275
* mesa: https://github.com/android-ia/external-mesa.git "master" branch 98b57dd99d603072da4e3dddf062de71303d4f84
* mingbm: https://github.com/01org/minigbm.git "master" barnch deb93aca440ab75828c847cb3cec97a1315a69e2
