#!/bin/bash
nfd
trap "pkill nfd; exit" SIGHUP SIGINT SIGTERM
