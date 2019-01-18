#!/usr/bin/python
import sys
import getopt
import docker
import subprocess
import os

client = docker.from_env()
containers_file = "containers.txt"


def create(video_folder_path):
    tag = "ndnrepo"
    client.images.build(path="./docker/", tag=tag)

    volume = {video_folder_path: {'bind': '/videos', 'mode': 'rw'}}

    ports = {80: 9292, 9696: 9696, 6363: 6363}
    container = client.containers.run(tag, detach=True, tty=True, privileged=True, volumes=volume, ports=ports)
    print("Container Name: %s" % container.name)

    print container.exec_run("/bin/bash -c 'nohup nfd-start > /var/log/nfd.out 2> /var/log/nfd_error.out < /dev/null &'", detach=True)
    print container.exec_run("/bin/bash -c 'sleep 5'", detach=False)
    print container.exec_run("/bin/bash -c 'nohup ndn-repo-ng > /var/log/repng.log 2> /var/log/repng_error.log < /dev/null &'", detach=True)
    print container.exec_run("/bin/bash -c 'chmod +x ./copy_files_to_ndn.sh && nohup ./copy_files_to_ndn.sh > /var/log/copy.log 2> /var/log/copy_error.log < /dev/null &'", detach=True)
    file = open(containers_file, "a+")
    file.write(container.name + "\n")
    file.close()


def destroy():
    print("destroying docker containers")

    if not os.path.isfile(containers_file):
        print("No container running")
        return

    with open(containers_file) as f:
        containers = f.readlines()

    for container in containers:
        client.containers.get(container.strip()).remove(force=True)

    f.close()
    os.remove(containers_file)


def main():
    myopts, args = getopt.getopt(sys.argv[1:], "hn:o:p:", ["operation=", "path="])

    operation_str = None
    video_folder_path = None

    for opt, arg in myopts:
        if opt == '-h':
            print("Usage: %s -o <run|destroy> -p videosPath" % sys.argv[0])
            sys.exit()
        elif opt in ("-p", "--path"):
            video_folder_path = arg
        elif opt in ("-o", "--operation"):
            operation_str = arg

    if 'run' == operation_str:
        if video_folder_path is None:
            print("please specify videos path")
        else:
            create(video_folder_path)
    elif 'destroy' == operation_str:
        destroy()
    else:
        print "invalid operation. See -h for details."


main()
