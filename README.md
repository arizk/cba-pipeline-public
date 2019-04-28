# Introduction

This is the evaluation software for "CBA: Context-based Adaptation" by Bastian Alt, Trevor Ballard, Ralf Steinmetz, Heinz Koeppl and Amr Rizk. It emulates an NDN network with clients, servers, and caches, in which clients stream video provided by the servers and stored by the caches. The streaming is headless, which means you can't watch the video, but we can log the progress of each client to see if their adaptation algorithms are choosing appropriate bitrates to avoid stalling while keeping the quality as high as possible. Clients can use one of several adaptation algorithms, including some novel contextual multi-armed bandit algorithms we created. The network bandwidth can also be shaped with `tc`, forcing the algorithms to pick lower or higher bitrates throughout the video.

The whole system can be divided into a few major, important components:

* **Main pipeline**: This is what starts up the NDN testbed, and where configuration options are specified. The main pipeline is run using Vagrant to keep setup simple.
* **QtSamplePlayer**: The video player. This is where you define your adaptation algorithms. For the contextual bandits, this is where the context vector is prepared and the bandits get used. If you want to change the context by, e.g., squaring all of the values, this is where you should do it.
* **NDN**: We use a slightly modified version of NDN to attach metadata to the Data packets, which goes into the context used by the contextual bandits. Right now, the only addition is `numHops`.
* **ndn-icp-download**: This is the interface to NDN. It sends out the Interests, receives the Data, and does other useful things like resending Interests on timeout. If you add more metadata to the Data packets, you will have to change this component to send that metadata to QtSamplePlayer.

Additionally, there are some other useful components for things like creating the bandwidth traces, creating a topology, and visualizing the data which I'll go over.

Note that this is research software that's been used for several projects. You will probably find things like folders, files, functions, and function parameters that aren't used anymore, but weren't removed. You can ignore these.

If you have any questions at all, feel free to email us. We are more than happy to help; this is a pretty complicated system.

# Installation

These installation instructions are for Ubuntu 18.04. No other environments have been tested.

## Prerequisites
The actual execution takes place with Vagrant, so install it with

    sudo apt update
    sudo apt install virtualbox vagrant

We also need to set up pip:

    sudo apt install python-pip

## Installing the pipeline
First, clone the cba-pipeline repo. It is several GBs because it has a lot of multimedia data, so it will probably take awhile:

    git clone https://github.com/arizk/cba-pipeline-public
	cd cba-pipeline-public

There will be a Vagrantfile in the containernet folder, so we can set it up and access it via

	cd containernet
    vagrant up
	vagrant ssh

Within Vagrant, the containernet repo will be at /home/ubuntu/containernet. Note that the files in this directory are the literal files on your machine, Vagrant is just a different execution environment. So if you change a file in this directory on your local machine, it will also be changed in Vagrant and vice-versa.

Once you are in Vagrant, you can build the Docker images and begin the program:

	cd /home/ubuntu/containernet/ndn-experiment
	sudo ./start.sh --build-docker

Once the program finishes, the results will be in containernet/results/. The first time you build will take a long time, but subsequent builds will use previous builds. You'll probably get errors for the ndn-cxx docs, just ignore these.

# Main Pipeline
## Overview
The testbed uses Containernet to do evaluations. This is an extension of Mininet that uses Docker containers. Every client, cache, and server is a Docker instance, and you can use and access them like any other Docker container.

## start.sh

start.sh is the program you run to do the evaluation. From within Vagrant, in `/home/ubuntu/containernet/ndn-experiment/`, assuming Docker has been built:

    sudo ./start.sh

start.sh optionally builds the Docker images, then runs the evaluation. Since each client, cache, and server is a Docker instance, we have to create the right Docker image before running the evaluation. The folder for the clients is containernet/ndn-containers/ndn\_headless-player, and the folder for caches and servers is containernet/ndn-containers/ndn-repo. A good rule of thumb is to rebuild the corresponding docker images any time you edit something in these repositories. You can call one of the following:

* `sudo ./start.sh --build-player`: rebuilds the client image
* `sudo ./start.sh --build-repo`: rebuilds the cache/server image
* `sudo ./start.sh --build-docker`: rebuilds everything; the client, cache, and server images

If you edit one of the adaptation algorithms, for example, you should do `sudo ./start.sh --build-player`

start.sh is set up to run multiple epochs of multiple configurations. Every value in `qoe_weights_list` and `algo_list` will be run for the number of epochs specified (the `seq` in the `for` loop). Furthermore, for contextual bandits, these algorithms will be run with memory and without memory. *With* memory means that the weights from one epoch will be used in the next, so the contextual bandit will continue to learn over epochs. *Without* memory means the bandit will start from nothing each epoch, which will test how well it can learn within just one video. One epoch means one video stream.

**Important**: start.sh calls docker\_ndn.py for each epoch, which is where the actual topology is deployed and the streaming begins. It gets called twice in start.sh, once with memory, and once without. If you want to use a different topology or change how long you stream the video, you **have** to edit **both** of the calls to docker\_ndn.py directly. The topology is given by the `-t` flag, and the duration (in seconds; 1 chunk is 2s, so 200s is 100 chunks) is given by the `-d` flag. We could not turn these into bash variables, as they would always be interpreted incorrectly, so they have to be changed directly.

## docker\_ndn.py

docker\_ndn.py is what deploys a topology, starts the streaming session, then tears down the topology at the end and stores the results, for one epoch.

The available topologies are defined in this file, in the `initExperiment` function. To create a new topology, follow the examples: create clients, caches, switches for the caches, and servers with `setNDNNodeInfo`, then connect them how you wish with `topology.addLink(...`. Note that every cache should be attached to a switch, and this switch should be attached to clients, servers, or other switches. You should not attach a cache to anything other than one switch, and you should not attach more than one cache to one switch. The bandwidth for each link is specified with the `bw` parameter in `topology.addLink(...`. If you use `tc`, the actual bandwidth on the link will change.

After the topology is chosen, `waitForPlayback` is called. This is where the streaming begins and ends.If you want to enable/disable `tc`, you can do so here by simply commenting/uncommenting the lines which create and start the `traffic_control` threads. Note that **you must manually specify which links tc should happen on.** Furthermore, you must create a tc thread for both directions of the link, as the tc only affects the outgoing data from the interface. In the args to `trafficControl`, the 2nd and 3rd argument are what's relevant here; the tc will be on the interface *from* the 2nd argument *to* the 3rd argument. Look at the current tc threads in `waitForPlayback` to see how this happens. Note that a lot of the arguments to this function are not currently used; they are historical, and were just not removed to save time. After the program begins, it will wait for the duration (specified by the `-d` flag) plus 20 seconds (if a client stalls, it will take a little longer than the duration to complete, so we give them some extra time), then fetch the results, kill the network, and end the epoch.

For each client in the topology, `waitForPlayback` will create a new thread to run `startPlayer`. This function does a few things:

1. Create the command that gets executed in the client Docker instance.
2. If we use memory, place the previous epoch's contextual bandit object (a Python pickle object with the weights from the last epoch) in the client Docker instance.
3. Execute the command, beginning playback.

After the duration plus 20 seconds, `waitForPlayback` will tell all of the clients to stop streaming and will call the `getDataFromClient` function for each client. This function will simply get the client's output and place it in the results folder. If we use memory, it will also get the contextual bandit object and store it to use for the next epoch.

## trafficControl.py and dynamicLinkChange.py

trafficControl.py does tc, modifying the bandwidth of a link over the duration of the video. The `traffic_control` function is the target of any tc threads made in `waitForPlayback`. It assumes that there are some bandwidth traces which are CSV files of the following form:

    <bandwidth in MBps>,<duration of bandwidth in seconds>

The actual `tc` command takes place in the `reconfigureConnection` function, which gets called here. You will probably not have to change anything in `dynamicLinkChange.py`. Note that, currently, all of the calls to `traffic_control` in `waitForPlayback` give the same client name, which means that all of the links will be reading from the same file (the one for "client1"), and will therefore modulate in the same way at the same time. You can have them read from different traces (if you want different links to change in different ways) by just changing this.

# QtSamplePlayer

qtsampleplayer is an open-source video player which we've modified. This is what does the streaming on the client Docker instances. It's a big piece of software, but we are mostly concerned with the adaptation algorithms, which are all the way in `containernet/ndn-containers/ndn_headless-player/code/ndn-dash/libdash/qtsampleplayer/libdashframework/Adaptation/`. Note that the code for the caches and servers is also in `containernet/ndn-containers/`, but you will probably not need to worry about them.

## Adaptation algorithms in C++

Each adaptation algorithm is a C++ file with some common functions. The contextual bandits are SparseBayesUcb.cpp, SparseBayesUcbOse.cpp, LinUcb.cpp, and SparseBayseUcbSvi.cpp. SparseBayseUcbSvi.cpp has a tendency to stall in the Python code which we couldn't figure out, so you will probably just want to use the first three.

Note that all of the calls to `std::cout` are what get written to the log file. We redirect the output to this file when executing the player.

Again, to be clear, if you change any of the C++ stuff, or really anything in a subdirectory of containernet/ndn-containers/, you **have** to run `sudo ./start.sh --build-player` or `sudo ./start.sh --build-docker` (again, build-player only rebuilds the clients, build-docker does both clients and caches/servers). When you do this, it will recompile the QtSamplePlayer code and put it in a new Docker image for the client. This is the easiest way to compile your code, and the output will tell you if there were any compilation errors (if you see any, press Ctrl+c a bunch of times to kill `start.sh` so you can look into the issue). A faster, but slightly more complicated way to check if a program compiles is detailed at the end of this document.

## OnSegmentDownload

The adaptation algorithms call functions from `run_bandits.py` to choose a bitrate and update the model. The main function in each C++ adaptation algorithm is `OnSegmentDownload`; this gets called whenever all of the Data packets for the last segment we requested have arrived, which means we can calculate the QoE, update our model, and use the context from these new Data packets. The structure of `OnSegmentDownload` is as follows:

1. Calculate the QoE for the previous segment.
2. Put all of the **previous** context information into Python lists, which will eventually be given to the bandits.
3. Execute the bandit's update function, which will update the internal model of the bandit with the reward, previous decision, and previous context. We use the previous context (as in, the context from when we made the previous decision, on the previous `OnSegmentDownload`) because it will tell the bandit "here's the information you had last time, here's the decision that you made, and here's the reward it got you".
4. Add the new context, which we'll use on the *next* call to `OnSegmentDownload`.
5. Call `SetBitrate`, which will tell the bandit to pick the next quality.
6. Call `NotifyBitrateChange`, which will tell a different part of QtSamplePlayer which bitrate it should select for the next segment. This is a bit of an implementation detail that you don't need to worry about.

## SetBitrate

`SetBitrate` follows a similar pattern as the middle of `OnSegmentDownload`:

1. Put all of the **new** context information into Python lists.
2. Execute the bandit's decision function, which will observe the context and return which quality we should request next.

## run\_bandits.py

run\_bandits.py is the interface we use to interact with the real contextual bandit code. You will almost certainly not have to change the real bandit code, but, if you change the context, you will have to change run\_bandits.py. There are three functions:

* `choose`: this is what we call in step 2 of `SetBitrate`.
* `update`: this is what we call in step 3 of `OnSegmentDownload`.
* `preprocess`: this is what you will probably need to change. We must have a fixed context vector size, but the number of Data packets can vary from one video chunk to another, so in this step we subsample the context to get the right number, and create the final context vector which will be given to the bandit.

## Changing the context

If you want to add a new set of context features, like ECN or the RTT squared, you will have to make a few changes. For manipulating existing context features (e.g. RTT squared), I would recommend you just square them and add them to the context vector in `preprocessing` in `run_bandits.py`. This is the easiest way and most logical to me, since we are just manipulating some information, which the C++ code doesn't need to know about.

To add something new, you will probably have to change the C++. Look at the way rttMap and numHopsMap are set up for inspiration. Keep in mind that each quality needs its own context vector, which is why we use maps. A useful command-line function for searching through many files is:

    find /path/to/folder -type f -exec grep --color -n -e "thing you want to search for" /dev/null {} +

Note that if you add something to NDN, e.g. more metadata like numHops, you will also have to change ..../libdashframework/Input/DASHReceiver.\* and ndn-icp-download. DASHReceiver is the interface between ndn-icp-download and the adaptation algorithm, so context from the network gets routed through it. 

# ndn-icp-download

ndn-icp-download, located in `containernet/ndn-containers/ndn_headless-player/code/ndn-icp-download/`, is the interface between NDN and the video player. Any metadata attached to the Data packets will have to be processed and sent to the video player here, in the `onDataReceived` function. Look at how `numHopsTag` is fetched and processed for ideas. Note also that you will have to update the signature for `signalContextToDASHReceiver` to accomodate the new information. In DASHReceiver.cpp, where we call `this->chunktimeObs->addContextSignal(...`, if you add another element to the context vector, append another underscore and number to the end of the `boost::bind(...`. For example, if you put 4 things in the call to `signalContextToDASHReceiver`, that line would read

    this->chunktimeObs->addContextSignal(boost::bind(&DASHReceiver::NotifyContextChunk, this, _1, _2, _3, _4));

Other than this, you probably don't need to worry about ndn-icp-download.

# NDN

There are two code components to NDN: there's ndn-cxx, which is a C++ library for working with NDN, and NFD (NDN Forwarding Daemon), which is the actual forwarder. If you want to add a new metadata field to the Data packets, you will have to change both of them. Trevor only added numHops, so either branch should just have a couple of commits. Look at these commits to see what was changed, as the process should be similar for any new kind of context.

These repositories are used to create the Docker images for the clients, caches, and servers. They get cloned and set up in the Dockerfile, so if you want to change the URL for some reason, edit the Dockerfile and rebuild the image. We assume that the NDN code is the same for clients, caches, and servers.

Final note, NDN **MUST** be at the version in the fork (0.4.1). If you try to use a newer version of NDN, it will not work. So if you just want to remove numHops, you should take our fork and roll back our commits.

To test the new NDN code, you have to include it in the Docker image. This happens automatically in the Dockerfiles for ndn\_headless-player and ndn\_repo: we clone the git repository and compile it. However, Docker will automatically use the stored result of each command when building to save time. This means that it will see that the git url and build instructions did not change, so it will just use the build results from last time even though the *contents* of the GitHub repo may changed. An easy way to get around this is to put some harmless command in the Dockerfile **above** the NDN clone commands, like `echo "hello world"`. Docker will see that the Dockerfile changed at this line, so it will have to re-execute all of the commands after it as well, which will cause it to pull and build our new NDN code.

NDN is very poorly documented, so if you have any trouble, please do not hesitate to email me. We found that the NDN mailing lists were the most useful resource, so you could try searching through them as well.

# Miscellaneous

## Visualizing the Results

The visualization is done via a Jupyter (i.e. IPython) notebook. For simplicity, it assumes you have a folder on your local machine where you keep all of the previous evaluation runs in separate folders. Within each run's folder, there's another folder with the bandwidth\_traces and the results. For example, say you keep all of the evaluations at ~/evaluations. Then, after a run, you would do:

    cd ~/evaluations
	mkdir <name of the run. The name itself isn't important but, each run should have a unique name>
	cd <name of the run>
	mkdir bandwidth_traces
	mkdir results
	cp -r ~/cba-pipeline/containernet/results/* results/
	cp -r ~/cba-pipeline/containernet/ndn-experiment/bandwidth_traces/* bandwidth_traces/

Now open Visualizations.ipynb. In the very first cell, specify the <name of the run>, the weight set you want to visualize, and whether you want to visualize the memory or non-memory results, along with directory paths, etc. The main visualizations will be made. At the end is a miscellaneous section to produce data for an additional visualization, the fairness graph, and tables in the paper.

The fairness graph measures how well two clients share a common linke, and is made by running

    python3 make_fairness_graph.py

Based on the weights/mem configuration used to output the fairness graph data, the filename of the data will be different. Edit make_fairness_graph.py and change the suffix to be correct.

## Generating Bandwidth Traces

In containernet/ndn-experiment/bandwidth_traces, there is a Python file, `generate_traces.py`, which can be used to generate bandwidth traces. Edit this file to generate as many values and files as you want or change the distribution they're drawn from. Then create the traces with

    python3 generate_traces.py

## Generating Full Topologies

You are free to use any topology you like (consult with Amr first), but this is the way we generate topologies with several clients/caches/servers that are all randomly attached to each other. In util/, the `generate_topology.py` script will create a semi-random undirected graph produce a picture of it. This graph is basically a network, and the clients, caches, and servers will be represented by different colors. The text output will tell you which things to connect to one another, and the image `ndn-topo.png` will show what the network looks like. Now you can edit `docker_ndn.py` and add your generated topology by hand, based on this output.

Note that topologies have to be fairly small since each node is a Docker instance, so the computer may run out of space. The values that are currently in `generate_topology.py` where about as high as we could get them on a standard machine.

## Working inside clients
When doing a lot of C++ stuff, it's sometimes faster to try things from within the client instance. If you kill start.sh while it's streaming something (e.g. by pressing Ctrl+c or Ctrl+\), the teardown code won't be called, so the Docker instances will persist indefinitely. From within Vagrant, you can then list all of the Docker instances and ssh into them:

    docker ps
    docker exec -it <name of container> /bin/bash

Once inside, you can edit, compile, and run the code to test it without having to wait for Docker to set up and tear down. We have two useful scripts for recompiling ndn-icp-download and qtsampleplayer from within a client called `rebuild-ndn-icp.sh` and `rebuild-player.sh`, which should be in the root of the client. There's another script `restart.sh`, that optionally rebuilds either of these things and restarts the player, so a frequent workflow would be to change something, call one of these scripts, then keep tweaking if it didn't compile, and finally make our changes in the local files before restarting the pipeline. Note that the code only exists within this Docker instance; it's not connected to the real file. So if you change something in the adaptation algorithms, once you kill Docker, your changes will be lost. Just make the same changes in your local file and you'll be good.
