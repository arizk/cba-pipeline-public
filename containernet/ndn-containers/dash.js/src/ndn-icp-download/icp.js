var DataPath = require('./data-path.js').DataPath;
var NdnIcpDownload = require('./ndn-icp-download.js').NdnIcpDownload;

const RECEPTION_BUFFER = 128000;
const DEFAULT_PATH_ID = 'default_path';
const P_MIN = 0.00001;
const DROP_FACTOR = 0.02;
const BETA = 0.5;
const GAMMA = 1;

/*
 * Size of RTT sample queue. RAQM Window adaptation starts only if this queue is full. Until it is full, Interests are sent one per Data Packet
 */
const SAMPLES = 30;
const DEFAULT_INTEREST_TIMEOUT = 1000000; // microsecond (us)
/*
const MAX_WINDOW = (RECEPTION_BUFFER - 1);
const MAX_SEQUENCE_NUMBER = 1000;
const P_MIN_STRING = "0.00001";
const DROP_FACTOR_STRING = "0.02";
const BETA_STRING = "0.5";

const ALLOW_STALE = false;

//const GOT_HERE() ((void)(__LINE__));
const TRUE = 1;
const FALSE = 0;
const MAX_INTEREST_LIFETIME_MS = 10000; // millisecond (ms)
const CHUNK_SIZE = 1024;
const RATE_SAMPLES = 4096; //number of packets on which rate is calculated
const PATH_REP_INTVL = 2000000;
const QUEUE_CAP = 200;
const THRESHOLD = 0.9;
const UINT_MAX = 4294967295;
*/

var process = function process(name, initial_chunk, nchunks, callback, offset, length) {

    console.log('executing icp.js: process');

    var allow_stale = false;
    var sysTimeout = -1;
    var InitialMaxwindow = RECEPTION_BUFFER - 1; // Constants declared inside ndn-icp-download.hpp
    var timer = DEFAULT_INTEREST_TIMEOUT;
    var drop_factor = DROP_FACTOR;
    var p_min = P_MIN;
    var beta = BETA;
    var gamma = GAMMA;
    var samples = SAMPLES;
    var output = false;
    var report_path = false;

    var ndnicpdownload = new NdnIcpDownload(callback, offset, length, InitialMaxwindow, gamma, beta, allow_stale, timer / 1000);

    var initPath = new DataPath(drop_factor, p_min, timer, samples);
    ndnicpdownload.insertToPathTable(DEFAULT_PATH_ID, initPath);

    ndnicpdownload.setCurrentPath(initPath);
    ndnicpdownload.setStartTimetoNow();
    ndnicpdownload.setLastTimeOutoNow();

    if (output)
        ndnicpdownload.enableOutput();
    if (report_path)
        ndnicpdownload.enablePathReport();
    if (sysTimeout >= 0)
        ndnicpdownload.setSystemTimeout(sysTimeout);

    console.log('NAME', name);
    console.log('calling download');

    ndnicpdownload.download(name, initial_chunk, nchunks);
};

exports.process = process;
