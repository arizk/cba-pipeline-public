import React, {Component} from 'react';
import _ from 'lodash';

// require("expose?ndn!../lib/ndn");

const styles = {
  container: {
    display: 'flex',
    flexDirection: 'column',
    minHeight: '100%'
  },
  main: {
    flex: 1,
    display: 'flex',
    flexDirection: 'column'
  }
};

let type; // Type of file
let codecs; //  Codecs allowed
let width; //  native width and height
let height;
let media; // media file names
//  elements

//  description of initialization segment, and approx segment lengths
let initialization;
let segDuration;
let vidDuration;
const ndnMode = true;
//  video parameters
let bandwidth; // bitrate of video
//  parameters to drive segment loop
let index = 1; // Segment to get
let segments;
let numSegments;
//  source and buffers
let mediaSource;
let videoSource;
let lowestRepresentation;
//  parameters to drive fetch loop
let segCheck;
let currentRepresentation;
const allBandwidths = []; // contains the adaptationSet bandwidths
let lastTime = 0;
let bufferUpdated = false;
// flags to keep things going
let lastMpd = "";
let xmlData; // stored mpd
let prefix; // AS prefix

let requestId = 0;
const hostip = "localhost"; // Hub IP address

let face = null;

let startTimer;
let measuredBandwidth;
const measuredBandwidthSamples = [];

// init variables
let curIndex, curBandwidth, msrBandwidth, simulationMode, segLength, slider, vTime, bTime, videoElement,
  playButton = null;

export class NDNPlayer extends Component {

  // Click event handler for load button
  componentDidMount() {
    // inject functions into scope
    SegmentFetcher.prototype.log = function (data) {
      console.log(data);
    };
    SegmentFetcher.prototype.getFileType = this.getFileType;
    SegmentFetcher.prototype.showTypes = this.showTypes;
    SegmentFetcher.prototype.setupVideo = this.setupVideo;
    window.initVideo = this.initVideo;
    window.onCompleteInit = this.onCompleteInit;
    window.updateFunct = this.updateFunct;
    window.getStarted = this.getStarted;
    window.updateFunct = this.updateFunct;
    window.playSegment = this.playSegment;
    window.onCompleteSegment = this.onCompleteSegment;
    window.getAverage = this.getAverage;
    window.renderVideoInfo = this.renderVideoInfo;
    window.fileChecks = this.fileChecks;
    window.formatTime = this.formatTime;
    window.closestRepresentation = this.closestRepresentation;
    window.pZ = this.pZ;
    window.reloadParameters = this.reloadParameters;
    window.showTypes = this.showTypes;
    window.loadInit = this.loadInit;
    window.onCompleteLoadInit = this.onCompleteLoadInit;
    window.resumeSegmentFetching = this.resumeSegmentFetching;
    window.continuePlayback = this.continuePlayback;

    face = new Face({
      host: hostip
    }); // face connection object

    curIndex = document.getElementById("curIndex"); // Playing segment
    curBandwidth = document.getElementById("curBandwidth"); // Selected Bandwidth
    msrBandwidth = document.getElementById("measuredBandwidth"); // Measured Bandwidth
    simulationMode = document.getElementById("simulationMode"); // Simulation Mode Switch
    segLength = document.getElementById("segLength");
    slider = document.getElementById("slider");
    vTime = document.getElementById("curTime");
    bTime = document.getElementById("bufTime");
    videoElement = document.getElementById('myVideo');
    playButton = document.getElementById("load");

    playButton.addEventListener("click", () => {

      if (videoElement.paused == true) {
        //  Retrieve mpd file, and set up video
        const curMpd = document.getElementById("filename").value;
        // const host = document.getElementById("hostip").value;

        if (curMpd != lastMpd) {
          window.cancelAnimationFrame(requestId);
          lastMpd = curMpd;
          this.getData(curMpd);
        } else {
          videoElement.play();
        }
      } else {
        videoElement.pause();
      }
    }, false);
    // Do a little trickery, start video when you click the video element
    videoElement.addEventListener("click", () => {
      playButton.click();
    }, false);
    //  Event handler for the video element errors
    document.getElementById("myVideo").addEventListener("error", e => {
      this.log(`video error: ${e.message}`);
    }, false);
  }

  // gets the mpd file and parses it
  onCompleteMpd(content) {
    console.log("MPD recieved. ");
    const tempoutput = content.buf().toString();
    const parser = new DOMParser(); //  create a parser object
    // Create an xml document from the .mpd file for searching
    xmlData = parser.parseFromString(tempoutput, "text/xml", 0);
    this.log("parsing mpd file");
    // Get and display the parameters of the .mpd file
    this.getFileType(xmlData);
    // set up video object, buffers, etc
    this.setupVideo();
    // initialize a few variables on reload
    // this.clearVars();
  }

  // catch fetching error
  onError(errorCode, message) {
    console.log(`Error: ${message}`);
  }

  getData(url) {
    if (url !== "") {
      if (ndnMode) {
        // look for the latest file version
        face.expressInterest(new Name(url), (interest, data) => {
          console.log(`getData - Received ${data.getName().toUri()}`);

          const version = data.getName().get(-1);
          console.log(`Version: ${version.toEscapedString()}`);

          // add version number to the video url
          //TODO: confirm this change from Denny
          //url += version.toEscapedString();

          // create interest for the target video
          interest = new Interest(new Name(url));
          interest.setInterestLifetimeMilliseconds(1000);

          console.log("interest: "+JSON.stringify(interest));

          // fetch all video segments
          SegmentFetcher.fetch(face, interest, SegmentFetcher.DontVerifySegment, this.onCompleteMpd, interest => {
            console.log("Timeout Error: unable to fetch video segments " + url);
            this.getData(url);
          });
        }, interest => {
          console.log("Timeout Error: Unable to expressInterest: "+ url);
          this.getData(url);
        });
      } else {
        // HTTP fetching:

        const xhr = new XMLHttpRequest(); // Set up xhr request
        xhr.open("GET", url, true); // Open the request
        xhr.responseType = "text"; // Set the type of response expected
        xhr.send();
        //  Asynchronously wait for the data to return
        xhr.onreadystatechange = function () {
          if (xhr.readyState == xhr.DONE) {
            const tempoutput = xhr.response;
            const parser = new DOMParser(); //  create a parser object
            // Create an xml document from the .mpd file for searching
            const xmlData = parser.parseFromString(tempoutput, "text/xml", 0);
            this.log("parsing mpd file");
            // Get and display the parameters of the .mpd file
            this.getFileType(xmlData);
            // set up video object, buffers, etc
            this.setupVideo();
            // initialize a few variables on reload
            // this.clearVars();
          }
        };
        //  Report errors if they happen during xhr
        xhr.addEventListener("error", e => {
          this.log(`Error: ${e} Could not load url.`);
        }, false);
      }
    } else {
      this.log("Error: Could not load mpd: URL is empty.");
    }
  }

  // retrieve parameters from our stored .mpd file
  getFileType(data) {
    // file = data.querySelectorAll("BaseURL")[0].textContent.toString();
    // console.log(file);
    const rep = data.querySelectorAll("Representation");
    const ada = data.querySelectorAll("AdaptationSet");
    // console.log(rep);
    type = ada[ada.length - 1].getAttribute("mimeType");
    numSegments = ada[ada.length - 1].getAttribute("numberOfSegments");
    // console.log(type);
    codecs = rep[rep.length - 1].getAttribute("codecs");
    // console.log(codecs);
    width = rep[rep.length - 1].getAttribute("width");
    // console.log(width);
    height = rep[rep.length - 1].getAttribute("height");
    // console.log(height);
    bandwidth = rep[rep.length - 1].getAttribute("bandwidth");

    // save available bandwidth values
    for (let bw = 0; bw < rep.length; bw++) {
      allBandwidths[bw] = rep[bw].getAttribute("bandwidth");
      // console.log(allBandwidths[bw]);
    }

    console.log(allBandwidths);
    // find the lowest representation
    let current = allBandwidths[0];
    let difference = Math.abs(0 - current);
    lowestRepresentation = 0;
    for (let val = 1; val < allBandwidths.length; val++) {
      const newdifference = Math.abs(0 - allBandwidths[val]);
      if (newdifference < difference) {
        difference = newdifference;
        current = allBandwidths[val];
        lowestRepresentation = val;
      }
    }

    const ini = data.querySelectorAll("SegmentTemplate");
    initialization = ini[ini.length - 1].getAttribute("initialization");
    initialization = initialization.replace(/!.*!/, rep[rep.length - 1].getAttribute("id"));
    // console.log(initialization);
    media = ini[ini.length - 1].getAttribute("media");
    media = media.replace(/!.*!/, rep[rep.length - 1].getAttribute("id"));
    // console.log(media);
    segDuration = ini[ini.length - 1].getAttribute("duration");
    prefix = ini[ini.length - 1].getAttribute("Prefix");

    if(prefix == ".") {
      let filename = document.getElementById("filename").value;
      filename = filename.substring(0, filename.length-1); //ignore last slash
      prefix = filename.substring(0, filename.lastIndexOf("/")+1); //TODO: remove hardcoded video
    }

    currentRepresentation = rep.length - 1;
    // console.log(segDuration);
    // segments = data.querySelectorAll("SegmentURL");
    // console.log(segments);

    // get the length of the video per the .mpd file
    //   since the video.duration will always say infinity
    // var period = data.querySelectorAll("Period");
    // var vidTempDuration = period[1].getAttribute("duration");
    // vidDuration = parseDuration(vidTempDuration); // display length
    // var segList = data.querySelectorAll("SegmentList");
    // segDuration = segList[1].getAttribute("duration");

    showTypes(); // Display parameters
  }

  // Display parameters from the .mpd file
  showTypes() {
    const display = document.getElementById("myspan");
    let spanData;
    spanData = `<h3>Representation parameters:</h3><ul><li>Media file: ${media}</li>`;
    spanData += `<li>Type: ${type}</li>`;
    spanData += `<li>Codecs: ${codecs}</li>`;
    spanData += `<li>Width: ${width} -- Height: ${height}</li>`;
    spanData += `<li>Bandwidth: ${bandwidth}</li>`;
    spanData += `<li>Initialization Segment: ${initialization}</li>`;
    spanData += `<li>Prefix: ${prefix}</li>`;
    spanData += `<li>Segment length: ${segDuration / 1000} seconds</li>`;
    // spanData += "<li>" + vidDuration + "</li>";
    spanData += "</ul>";
    display.innerHTML = spanData;
  }

  renderVideoInfo() {
    // display current video position
    vTime.innerText = formatTime(videoElement.currentTime);
    // display current bandwidth slider position
    // curBandwidth.innerText = slider.value;
    // display measured bandwidth
    msrBandwidth.innerText = measuredBandwidth;
    // display video buffer status
    if (index > 2) {
      bTime.innerText = videoElement.buffered.end(0);
    }
    // recall this function when available
    requestId = window.requestAnimationFrame(renderVideoInfo);
  }

  //  create mediaSource and initialize video
  setupVideo() {
    //  this.clearLog(); // Clear console log
    // create the media source
    mediaSource = new (window.MediaSource || window.WebKitMediaSource)();
    const url = URL.createObjectURL(mediaSource);

    videoElement.pause();
    videoElement.src = url;
    videoElement.width = width;
    videoElement.height = height;
    // Wait for event that tells us that our media source object is
    //   ready for a buffer to be added.
    const self = this;
    mediaSource.addEventListener('sourceopen', e => {
      try {
        videoSource = mediaSource.addSourceBuffer(`${type}; codecs="${codecs}"`);
        initVideo(initialization, prefix);
      } catch (e) {
        console.log('Exception calling addSourceBuffer for video:'+ JSON.stringify(e));
        return;
      }
    });
    // handler to switch button text to Play
    videoElement.addEventListener("pause", () => {
      playButton.innerText = "Play";
    }, false);
    // handler to switch button text to pause
    videoElement.addEventListener("playing", () => {
      playButton.innerText = "Pause";
    }, false);
    // remove the handler for the timeupdate event
    videoElement.addEventListener("ended", () => {
      videoElement.removeEventListener("timeupdate", this.checkTime);
    }, false);
  }

  // call this after successfully fetching the Init segment
  onCompleteInit(content) {
    console.log("Init Segment recieved. ");
    const ini = content.buf();
    // add response to buffer
    try {
      videoSource.appendBuffer(ini);
      // Wait for the update complete event before continuing
      videoSource.addEventListener("update", updateFunct, false);
    } catch (e) {
      this.log('Exception while appending initialization content', e);
    }
  }

  //  Load video's initialization segment
  initVideo(init, url) {
    const self = this;
    if(url == ""){
      this.log("Error: Could not load init segment.");
      return
    }

    let link = url +init; //TODO: Remove hardcoded video value

    console.log("initVideo - link is :"+link);
    // look for the latest file version
    face.expressInterest(new Name(link), (interest, data) => {
      console.log(`initVideo - Received ${data.getName().toUri()}`);

      const version = data.getName().get(-1);
      console.log(`Version: ${version.toEscapedString()}`);

      // add version number to the video url
      //link += version.toEscapedString();

      // create interest for the target video
      interest = new Interest(new Name(link));
      interest.setInterestLifetimeMilliseconds(1000);

      // fetch all video segments
      SegmentFetcher.fetch(face, interest, SegmentFetcher.DontVerifySegment, onCompleteInit, interest => {
        console.log("initVideo - Unable to fetch segments: "+ link);
        self.initVideo(init, url);
      });
    }, interest => {
      console.log("initVideo - Unable to expressInterest for link : " + link);
      self.initVideo(init, url);
    });

    /*
     * HTTP Init:
     *
     var xhr = new XMLHttpRequest();
     if (range || url) { // make sure we've got incoming params
     //  set the desired range of bytes we want from the mp4 video file
     xhr.open('GET', url);
     xhr.setRequestHeader("Range", "bytes=" + range);
     segCheck = (timeToDownload(range) * .8).toFixed(3); // use .8 as fudge factor
     xhr.send();
     xhr.responseType = 'arraybuffer';
     try {
     xhr.addEventListener("readystatechange", function () {
     if (xhr.readyState == xhr.DONE) { // wait for video to load
     // add response to buffer
     try {
     videoSource.appendBuffer(new Uint8Array(xhr.response));
     // Wait for the update complete event before continuing
     videoSource.addEventListener("update", updateFunct, false);
     } catch (e) {
     log('Exception while appending initialization content', e);
     }
     }
     }, false);
     } catch (e) {
     log(e);
     }
     } else {
     return // No value for range or url
     }
     */
  }

  updateFunct() {
    console.log("Starting playback. ");
    //  This is a one shot function, when init segment finishes loading,
    //    update the buffer flat, call getStarted, and then remove this event.
    bufferUpdated = true;
    getStarted(media); // Get video playback started
    videoSource.removeEventListener("update", updateFunct);
  }

  //  Play our file segments
  getStarted(url) {
    console.log(`Getting Segment ${index}`);
    //  Start by loading the first segment of media
    playSegment(index, url);
    // start showing video time
    requestId = window.requestAnimationFrame(renderVideoInfo);
    // display current index
    curIndex.textContent = index + 1;
    index++;
    //  Continue in a loop where approximately every x seconds reload the buffer
    videoElement.addEventListener("timeupdate", fileChecks, false);
  }

  // get closest representation
  closestRepresentation(bw) {
    let current = allBandwidths[lowestRepresentation];
    let difference = Math.abs(bw - current);
    let representation = lowestRepresentation;
    // console.log(lowestRepresentation);

    for (let val = 0; val < allBandwidths.length; val++) {
      if (val != lowestRepresentation) {
        const newdifference = bw - allBandwidths[val];
        if (newdifference >= 0) {
          if (newdifference < difference) {
            difference = newdifference;
            current = allBandwidths[val];
            representation = val;
          }
        }
      }
    }
    return representation;
  }

  // load a new representation
  reloadParameters(repId) {
    const rep = xmlData.querySelectorAll("Representation");
    console.log(rep);

    // console.log(type);
    codecs = rep[repId].getAttribute("codecs");
    // console.log(codecs);
    width = rep[repId].getAttribute("width");
    // console.log(width);
    height = rep[repId].getAttribute("height");
    // console.log(height);
    bandwidth = rep[repId].getAttribute("bandwidth");

    // console.log(bandwidth);
    const ini = xmlData.querySelectorAll("SegmentTemplate");
    initialization = ini[ini.length - 1].getAttribute("initialization");
    initialization = initialization.replace(/!.*!/, rep[repId].getAttribute("id"));
    // console.log(initialization);
    media = ini[ini.length - 1].getAttribute("media");
    media = media.replace(/!.*!/, rep[repId].getAttribute("id"));
    // console.log(media);

    currentRepresentation = repId;
    // console.log(segDuration);
    // segments = data.querySelectorAll("SegmentURL");
    // console.log(segments);

    // get the length of the video per the .mpd file
    //   since the video.duration will always say infinity
    // var period = data.querySelectorAll("Period");
    // var vidTempDuration = period[1].getAttribute("duration");
    // vidDuration = parseDuration(vidTempDuration); // display length
    // var segList = data.querySelectorAll("SegmentList");
    // segDuration = segList[1].getAttribute("duration");

    showTypes(); // Display parameters
  }

  // call after getting the init segment
  onCompleteLoadInit(content) {
    console.log("Init Segment recieved. ");
    const ini = content.buf();
    // add response to buffer
    try {
      videoSource.appendBuffer(ini);
      // Wait for the update complete event before continuing
      videoSource.addEventListener("update", resumeSegmentFetching, false);
    } catch (e) {
      this.log('Exception while appending initialization content', e);
    }
  }

  loadInit(init, url) {
    if (url !== "") {
      let link = url + init;
      // look for the latest file version
      face.expressInterest(new Name(link), (interest, data) => {
        console.log(`Receiveed ${data.getName().toUri()}`);

        const version = data.getName().get(-1);
        console.log(`Version: ${version.toEscapedString()}`);

        // add version number to the video url
        //link += version.toEscapedString();

        // create interest for the target video
        interest = new Interest(new Name(link));
        interest.setInterestLifetimeMilliseconds(1000);

        // fetch all video segments
        SegmentFetcher.fetch(face, interest, SegmentFetcher.DontVerifySegment, onCompleteLoadInit, interest => {
          console.log("Timeout Error: Sending New Request : " + url);
          loadInit(init, url);
        });
      }, interest => {
        console.log("Timeout Error: Sending New Request "+ url);
        loadInit(init, url);
      });
    } else {
      this.log("Error: Could not load init segment.");
    }
    /*
     * HTTP Init:
     *
     var xhr = new XMLHttpRequest();
     if (range || url) { // make sure we've got incoming params
     //  set the desired range of bytes we want from the mp4 video file
     xhr.open('GET', url);
     xhr.setRequestHeader("Range", "bytes=" + range);
     segCheck = (timeToDownload(range) * .8).toFixed(3); // use .8 as fudge factor
     xhr.send();
     xhr.responseType = 'arraybuffer';
     try {
     xhr.addEventListener("readystatechange", function () {
     if (xhr.readyState == xhr.DONE) { // wait for video to load
     // add response to buffer
     try {
     videoSource.appendBuffer(new Uint8Array(xhr.response));
     // Wait for the update complete event before continuing
     videoSource.addEventListener("update", updateFunct, false);
     } catch (e) {
     log('Exception while appending initialization content', e);
     }
     }
     }, false);
     } catch (e) {
     log(e);
     }
     } else {
     return // No value for range or url
     }
     */
  }

  //  get video segments
  fileChecks() {
    // If we're ok on the buffer, then continue
    if (bufferUpdated == true) {
      if (index < numSegments) {
        //  loads next segment when time is close to the end of the last loaded segment
        if ((videoElement.currentTime - lastTime) >= segCheck) {
          console.log('Simulation Mode Disabled');
          const wantedRepresentation = closestRepresentation(measuredBandwidth);

          // console.log(wantedRepresentation);
          // console.log(slider.value);

          if (wantedRepresentation === currentRepresentation) {
            playSegment(index, media);
            lastTime = videoElement.currentTime;
            curIndex.textContent = index + 1; // display current index
            index++;
          } else {
            videoElement.removeEventListener('timeupdate', fileChecks, false);
            reloadParameters(wantedRepresentation);
            loadInit(initialization, prefix); // get rep. initialization segment
          }
        }
      } else {
        videoElement.removeEventListener("timeupdate", fileChecks, false);
      }
    }
  }

  continuePlayback(url) {
    console.log(`Getting Segment ${index}`);
    //  Start by loading the first segment of media after Representation change
    playSegment(index, url);

    // display current index
    curIndex.textContent = index + 1;
    index++;
    //  Continue in a loop where approximately every x seconds reload the buffer
    videoElement.addEventListener("timeupdate", fileChecks, false);
  }

  resumeSegmentFetching() {
    console.log("Representation Changed. ");
    //  This is a one shot function, when init segment finishes loading,
    //    update the buffer flat, call getStarted, and then remove this event.
    bufferUpdated = true;
    continuePlayback(media); // Continue video playback
    videoSource.removeEventListener("update", resumeSegmentFetching);
  }

  getAverage(bwArray) {
    let totalBw = 0;
    for (let i = 0; i < bwArray.length; i++) {
      totalBw += bwArray[i];
    }
    return (totalBw / bwArray.length);
  }

  // call this after successfully fetching a segment
  onCompleteSegment(content) {
    const endTimer = new Date().getTime();
    const downloadTime = (endTimer - startTimer) / 1000;
    const seg = content.buf();
    console.log(`Segment recieved in ${downloadTime} seconds. length: ${seg.length} bytes`);

    // compute measured bandwidth
    const cb = (seg.length / downloadTime) * 8;
    measuredBandwidthSamples.push(cb);
    if (measuredBandwidthSamples.length > 3) {
      measuredBandwidthSamples.splice(0, 1);
    }
    measuredBandwidth = getAverage(measuredBandwidthSamples);

    // Add received content to the buffer
    try {
      videoSource.appendBuffer(seg);
    } catch (e) {
      this.log('Exception while appending', e);
    }

    //  Calculate when to get next segment based on time of current one
    const segL = segDuration / 1000 * 0.8; // use .8 as fudge factor

    if (index > 2) {
      const bufferNotPlayed = videoElement.buffered.end(0) - videoElement.currentTime;

      // Keep a maximum of three not played segments in the buffer
      if (bufferNotPlayed > (3 * segL)) {
        segCheck = bufferNotPlayed - (3 * segL);
      } else {
        segCheck = 0; // get next segment asap
      }
    } else {
      segCheck = 0; // get next segment asap
    }

    segLength.textContent = segCheck;
  }

  //  Play segment plays a segment of a media file
  playSegment(index, url) {
    console.log("url to play is "+url);
    if (url !== "") {
      url = url.replace(/§.*§/, index);
      let link = prefix + url;
      // look for the latest file version
      face.expressInterest(new Name(link), (interest, data) => {
        console.log(`PlaySegment- Received ${data.getName().toUri()}`);

        const version = data.getName().get(-1);
        console.log(`Version: ${version.toEscapedString()}`);

        // add version number to the segment url
        //link += version.toEscapedString();

        // create interest for the target segment
        interest = new Interest(new Name(link));
        interest.setInterestLifetimeMilliseconds(1000);

        // fetch all video chunks
        SegmentFetcher.fetch(face, interest, SegmentFetcher.DontVerifySegment, onCompleteSegment, interest => {
          console.log("Timeout Error: Sending New Request " + url);
          playSegment(index, url);
        });
        startTimer = new Date().getTime();
      }, interest => {
        console.log("Timeout Error: Sending New Request "+ url);
        playSegment(index, url);
      });
    } else {
      this.log("Error: Could not load init segment.");
    }
  }

  log(s) {
    //  send to console
    //    you can also substitute UI here
    console.log(s);
  }

  //  Clears the log
  clearLog() {
    console.clear();
  }

  clearVars() {
    index = 1;
    lastTime = 0;
  }

  timeToDownload(range) {
    const vidDur = range.split("-");
    // time = size * 8 / bitrate
    return (((vidDur[1] - vidDur[0]) * 8) / bandwidth);
  }

  // converts mpd time to human time
  parseDuration(pt) {
    // parse time from format "PT#H#M##.##S"
    let ptTemp = pt.split("T")[1];
    ptTemp = ptTemp.split("H");
    const hours = ptTemp[0];
    const minutes = ptTemp[1].split("M")[0];
    const seconds = ptTemp[1].split("M")[1].split("S")[0];
    const hundredths = seconds.split(".");
    //  Display the length of video (taken from .mpd file, since video duration is infinate)
    return `Video length: ${hours}:${pZ(minutes, 2)}:${pZ(hundredths[0], 2)}.${hundredths[1]}`;
  }

  //  Converts time in seconds into a string HH:MM:SS.ss
  formatTime(timeSec) {
    let seconds = timeSec % 60; //  get seconds portion
    const minutes = ((timeSec - seconds) / 60) % 60; //  get minutes portion
    const hours = ((timeSec - seconds - (minutes * 60))) / 3600; //  get hours portion
    seconds = seconds.toFixed(2); // Restrict to 2 places (hundredths of seconds)
    const dispSeconds = seconds.toString().split(".");
    return (`${pZ(hours, 2)}:${pZ(minutes, 2)}:${pZ(dispSeconds[0], 2)}.${pZ(dispSeconds[1], 2)}`);
  }

  //  Pad digits with zeros if needed
  pZ(value, padCount) {
    let tNum = `${value}`;
    while (tNum.length < padCount) {
      tNum = `0${tNum}`;
    }
    return tNum;
  }

  render() {
    return (
      <div style={styles.container}>
        <main style={styles.main}>
          <div className="row">
            <div className="col-md-12">
              <div className="form-group is-empty">

                <div>

                  <input id="filename" type="text" className="form-control control"
                         defaultValue="/ndn/broadcast/ndnfs/videos/tears_of_steel/stream.mpd/"/>
                  <input id="hostip" type="text" className="form-control control" defaultValue={hostip} readonly/>
                  <span className="input-group-btn">
                            <button id="load" type="button" className="btn">Play</button>
                          </span>
                </div>
              </div>
            </div>

          </div>
          <div className="row">
            <div className="col-md-12">
              <div className="embed-responsive embed-responsive-16by9">
                <video className="video embed-responsive-item" id="myVideo" autoPlay="autoplay">No video available
                </video>
              </div>
            </div>
          </div>
          <div className="row">
            <div className="col-md-12">
              <div className="panel-group" id="accordion" role="tablist" aria-multiselectable="true">
                <div className="panel panel-default">
                  <div className="panel-heading" role="tab" id="headingOne">
                    <h4 className="panel-title">
                      <a role="button" data-toggle="collapse" data-parent="#accordion" href="#collapseOne"
                         aria-expanded="false" aria-controls="collapseOne"
                      >
                        Show Stats
                      </a>
                    </h4>
                  </div>
                  <div id="collapseOne" className="panel-collapse collapse" role="tabpanel"
                       aria-labelledby="headingOne"
                  >
                    <div className="panel-body">
                      <ul>
                        <li>Measured bandwidth value: <span id="measuredBandwidth"></span> bps</li>
                      </ul>
                      <div id="curInfo"></div>
                      <h3>Fetching values:</h3>
                      <span id="myspan"/>
                      <ul>
                        <li>Segment index: <span id="curIndex"></span> of <span id="numIndexes"></span></li>
                        <li>Delay to next segment: <span id="segLength"></span></li>
                      </ul>
                      <ul>
                        <li>Video time: <span id="curTime"></span></li>
                        <li>Buffer length: <span id="bufTime"></span> seconds</li>
                      </ul>
                    </div>
                  </div>
                </div>
              </div>
            </div>
          </div>
        </main>
      </div>
    );
  }
}
