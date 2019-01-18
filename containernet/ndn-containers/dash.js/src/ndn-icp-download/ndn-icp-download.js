var PendingInterest = require('./pending-interest.js').PendingInterest;
var Interest = require('../../ndn-js/js/interest.js').Interest;
var Name = require('../../ndn-js/js/name.js').Name;
var Face = require('../../ndn-js/js/face.js').Face;
var micro = require('microseconds');

const RECEPTION_BUFFER = 128000;
const DEFAULT_PATH_ID = 'default_path';
const P_MIN = 0.00001;

/*
 * Size of RTT sample queue. RAQM Window adaptation starts only if this queue is full. Until it is full, Interests are sent one per Data Packet
 */
const SAMPLES = 30;
const UINT_MAX = 4294967295;
/*
const MAX_WINDOW = (RECEPTION_BUFFER - 1);
const MAX_SEQUENCE_NUMBER = 1000;
const P_MIN_STRING = "0.00001";
const DROP_FACTOR = 0.02;
const DROP_FACTOR_STRING = "0.02";
const BETA = 0.5;
const BETA_STRING = "0.5";
const GAMMA = 1;

//const ALLOW_STALE = false;

//const GOT_HERE() ((void)(__LINE__));
const TRUE = 1;
const FALSE = 0;
const DEFAULT_INTEREST_TIMEOUT = 1000000; // microsecond (us)
const MAX_INTEREST_LIFETIME_MS = 10000; // millisecond (ms)
const CHUNK_SIZE = 1024;
const RATE_SAMPLES = 4096; //number of packets on which rate is calculated
const PATH_REP_INTVL = 2000000;
const QUEUE_CAP = 200;
const THRESHOLD = 0.9;
*/

function mergeUint8Arrays(a, b) {
    // Checks for truthy values on both arrays
    if (!a && !b) throw 'Please specify valid arguments for parameters a and b.';

    // Checks for truthy values or empty arrays on each argument
    // to avoid the unnecessary construction of a new array and
    // the type comparison
    if (!b || b.length === 0) return a;
    if (!a || a.length === 0) return b;

    // Make sure that both typed arrays are of the same type
    if (Object.prototype.toString.call(a) !== Object.prototype.toString.call(b))
        throw 'The types of the two arguments passed for parameters a and b do not match.';

    var c = new Uint8Array(a.length + b.length);
    c.set(a);
    c.set(b, a.length);

    return c;
}

var NdnIcpDownload = function NdnIcpDownload(callback, offset, length, pipeSize, gamma, beta, allowStale, lifetime_ms, scope) {
    this.callback = callback;
    this.m_offset = offset;
    this.m_length = length;
    //default
    this.m_maxwindow = pipeSize;
    this.m_Pmin = P_MIN;
    this.m_finalchunk = UINT_MAX;
    this.m_Samples = SAMPLES;
    this.m_GAMMA = gamma;
    this.m_BETA = beta;
    this.m_allow_stale = allowStale;
    this.m_defaulInterestLifeTime = lifetime_ms; //to keep consistent with the definition in make_template in ccncatchunks3
    this.m_scope = scope;
    this.m_isOutputEnabled = false;
    this.m_isPathReportEnabled = false;
    this.m_winrecvd = 0;
    this.m_winpending = 0;
    this.m_curwindow = 1; //size_t m_pipeSize;
    this.m_firstInFlight = 0;
    this.m_pending_interest_count = 0;
    this.m_next_pending_interest = 0; //size_t m_nextSegment;
    this.m_finalSlot = UINT_MAX;
    this.m_interests_sent = 0;
    this.m_pkts_recvd = 0;
    this.m_pkts_bytes_recvd = 0;
    this.m_raw_data_bytes_recvd = 0; //size_t m_totalSize;
    this.m_pkts_delivered = 0;
    this.m_raw_data_delivered_bytes = 0;
    this.m_pkts_delivered_bytes = 0;
    this.m_isAbortModeEnabled = false;
    this.m_winretrans = 0;
    this.m_download_completed = false;
    this.m_reTxLimit = 0;
    this.m_nHoles = 0;
    this.m_old_npackets = 0;
    this.m_old_rate = 0.0;
    this.m_sent_interests = 0;
    this.m_old_interests = 0;
    this.m_old_data_inpackets = 0;
    this.m_interests_inpackets = 0;
    this.m_old_interests_inpackets = 0;
    this.m_isSending = false;
    this.m_isRetransmission = false;
    this.m_is_first = false;
    this.m_timeout = 0;        //system timeout, default to run forever
    this.m_nTimeouts = 0;
    this.m_cur_path = null;
    this.m_dataName = new Name();
    this.m_firstDataName = new Name();
    this.m_dropFactor = null;
    this.m_saved_path = null;
    this.m_start_tv = null;
    this.m_stop_tv = null;
    this.m_last_timeout = null;
    this.m_start_period = null;
    this.m_PathTable = new Map();
    this.m_inFlightInterests = new Array(RECEPTION_BUFFER).fill(new PendingInterest());
    this.rec_buffer = null;
    //this.m_face = new Face({host: "localhost"});
    this.m_face = new Face({host: 'ws://localhost:9696'});
    if (typeof WIRELESS_RTT !== 'undefined') {
        this.m_seed = null;
        this.m_rtt_red = null;
        this.m_filePlot = null;
        this.m_observation_period = null;
        this.m_stop_tracing = null;
    }
};

exports.NdnIcpDownload = NdnIcpDownload;

NdnIcpDownload.prototype.setCurrentPath = function (path) {
    this.m_cur_path = path;
    this.m_saved_path = path;
};

NdnIcpDownload.prototype.setSystemTimeout = function (i) {
    this.m_timeout = i;
};

NdnIcpDownload.prototype.insertToPathTable = function (key, path) {
    if (this.m_PathTable.get(key) === undefined) {
        this.m_PathTable.set(key, path);
    }
    else {
        console.log('ERROR:failed to insert path to path table, the path entry already exists');
    }
};

NdnIcpDownload.prototype.setLastTimeOutoNow = function () {
    this.m_last_timeout = micro.now();
};

NdnIcpDownload.prototype.setStartTimetoNow = function () {
    this.m_start_tv = micro.now();
};

NdnIcpDownload.prototype.updateRTT = function (slot) {
    if (!this.m_cur_path) {
        console.log('ERROR: no current path found, exit');
        return;             //exit(EXIT_FAILURE);
    }
    else {
        var rtt;
        var now = micro.now();
        var sendTime = this.m_inFlightInterests[slot].getSendTime();
        rtt = now - sendTime;

        this.m_cur_path.insertNewRtt(rtt);
        this.m_cur_path.smoothTimer();
    }
};

NdnIcpDownload.prototype.increaseWindow = function () {
    if (this.m_winrecvd == this.m_curwindow) {
        if (this.m_curwindow < this.m_maxwindow) {
            this.m_curwindow += this.m_GAMMA;
        }

        this.m_winrecvd = 0;
    }
};

NdnIcpDownload.prototype.RAQM = function () {
    if (!this.m_cur_path) {
        console.log('ERROR: no current path found, exit');
        return;             //exit(EXIT_FAILURE);
    }
    else {
        // Change drop probability according to RTT statistics
        this.m_cur_path.updateDropProb();

        if (parseInt(Math.random() * 10000) <= (this.m_cur_path.getDropProb() * 10000)) {
            this.m_curwindow = this.m_curwindow * this.m_BETA;
            if (this.m_curwindow < 1)
                this.m_curwindow = 1;

            this.m_winrecvd = 0;
        }
    }
};

NdnIcpDownload.prototype.afterDataReception = function (slot) {
    //update win counters
    this.m_winpending--;
    this.m_winrecvd++;
    this.increaseWindow();

    this.updateRTT(slot);
    //set drop probablility and window size accordingly
    this.RAQM();
};

NdnIcpDownload.prototype.onData = function (interest, data) {
    /*
     * TODO
     *
     * In this part we should get the wireless RTT tag, but it is still not
     * available (we have to modify the forwarder abd make it able to recognize
     * wireless faces)
     */

    var finalBlockId = data.getMetaInfo().getFinalBlockId();
    if (finalBlockId.isSegment() && finalBlockId.toSegment() < this.m_finalchunk) {
        this.m_finalchunk = finalBlockId.toSegment();
        this.m_finalchunk++;
        this.m_download_completed = true;
    }

    var content = data.getContent();
    var name = data.getName();

    var dataSize = content.size();//////////////value_size
    var packetSize = data.wireEncode().size();

    var seq = name.components[name.size() - 1].toSegment();
    var slot = seq % RECEPTION_BUFFER;

    if (this.m_isAbortModeEnabled) {
        console.log('TODO: Map of retransmissions for abort mode (if useful)');
        // TODO Map of retransmissions for abort mode (if useful)
    }

    if (this.m_isOutputEnabled) {
        console.log('data received, seq number #', seq);
        console.log(content.buf().toString(), content.size());
    }

    //this.GOT_HERE();
    var pathId;
    if (typeof PATHID !== 'undefined') {
        //#ifdef PATHID
        var pathIdBlock = data.getPathId();

        if (pathIdBlock.empty()) {
            console.log('[ERROR]:path ID lost in the transmission.');
            return;                     //exit(EXIT_FAILURE);
        }

        pathId = data.getPathId().value();

    } else {
        //#else
        pathId = 0;
        //#endif
    }

    if (this.m_PathTable.get(pathId) === undefined) {
        if (this.m_cur_path) {
            //  Create a new path with some default param
            if (this.m_PathTable.size === 0) {     //empty()
                console.log('No path initialized for path table, error could be in default path initialization.');
                return;             //exit(EXIT_FAILURE);
            }
            else {
                //initiate the new path default param
                var newPath = this.m_PathTable.get(DEFAULT_PATH_ID);    //at(DEFAULT_PATH_ID);///shared_ptr
                //insert the new path into hash table
                this.m_PathTable.set(pathId, newPath);
                //this.m_PathTable[pathIdString] = newPath;
            }
        }
        else {
            console.log('UNEXPECTED ERROR: when running,current path not found.');
            return;             //exit(EXIT_FAILURE);
        }
    }

    this.m_cur_path = this.m_PathTable.get(pathId);

    //  Update measurements
    this.m_pkts_recvd++;
    this.m_raw_data_bytes_recvd += dataSize;
    this.m_pkts_bytes_recvd += packetSize;  // Data won't be encoded again since its already encoded before received

    //  Update measurements for path
    this.m_cur_path.updateReceivedStats(packetSize, dataSize);

    var nextSlot = this.m_next_pending_interest % RECEPTION_BUFFER;

    if (nextSlot > this.m_firstInFlight && (slot > nextSlot || slot < this.m_firstInFlight)) {
        console.log('out of window data received at # ', slot);
        return;
    }
    if (nextSlot < this.m_firstInFlight && (slot > nextSlot && slot < this.m_firstInFlight)) {
        console.log('out of window data received at # ', slot);
        return;
    }

    if (seq == (this.m_finalchunk - 1)) {
        this.m_finalSlot = slot;
        //GOT_HERE();
    }

    if (slot != this.m_firstInFlight) { //  Out of order data received, save it for later.
        if (!this.m_inFlightInterests[slot].checkIfDataIsReceived()) {
            //GOT_HERE();
            this.m_pending_interest_count++;
            this.m_inFlightInterests[slot].addRawData(data);
            this.m_inFlightInterests[slot].markAsReceived(true);
            //#ifdef WIRELESS_RTT
            if (typeof WIRELESS_RTT !== 'undefined') {
                this.afterDataReceptionWithReducedRTT(slot /*, RTT*/);
            }
            //#else
            else {
                this.afterDataReception(slot);
            }
            //#endif
        }
    }
    else { // In order data arrived
        //assert(!this.m_inFlightInterests[slot].checkIfDataIsReceived());
        this.m_pkts_delivered++;
        this.m_raw_data_delivered_bytes += dataSize;
        this.m_pkts_delivered_bytes += packetSize;

        // Save data to the reception buffer

        this.rec_buffer = mergeUint8Arrays(this.rec_buffer, content.buf());

        this.afterDataReception(slot);

        if (slot == this.m_finalSlot || this.m_download_completed === true) {
            if (this.callback) {
                if (this.m_offset !== null && this.m_length !== null) {
                    var buf1 = new Uint8Array(this.rec_buffer.slice(this.m_offset, this.m_offset + this.m_length));
                    this.callback(buf1);
                }
                else {
                    this.callback(this.rec_buffer);
                }
            }
            this.m_face.close();        //shutdown();
            return;
        }

        slot = (slot + 1) % RECEPTION_BUFFER;
        this.m_firstInFlight = slot;

        /*
         * Consume out-of-order pkts already received until there is a hole
         */
        while (this.m_pending_interest_count > 0 && this.m_inFlightInterests[slot].checkIfDataIsReceived()) {

            this.m_pkts_delivered++;
            this.m_raw_data_delivered_bytes += this.m_inFlightInterests[slot].getRawDataSize();
            this.m_pkts_delivered_bytes += this.m_inFlightInterests[slot].getPacketSize();

            this.rec_buffer = mergeUint8Arrays(this.rec_buffer, this.m_inFlightInterests[slot].getRawData());
            if (slot == this.m_finalSlot || this.m_download_completed === true) {
                //GOT_HERE();
                if (this.callback) {
                    if (this.m_offset !== null && this.m_length !== null) {
                        var buf2 = new Uint8Array(this.rec_buffer.slice(this.m_offset, this.m_offset + this.m_length));
                        this.callback(buf2);
                    }
                    else {
                        this.callback(this.rec_buffer);
                    }
                }
                this.m_face.close();        //shutdown();
                return;
            }

            this.m_inFlightInterests[slot].removeRawData();
            slot = (slot + 1) % RECEPTION_BUFFER;
            this.m_pending_interest_count--;
        }

    }

    /** Ask for the next one or two */
    while ((this.m_winpending < this.m_curwindow) && (this.m_next_pending_interest < this.m_finalchunk)) {
        this.ask_more(this.m_next_pending_interest++);
    }
    //GOT_HERE();
};

NdnIcpDownload.prototype.re_initialize = function () {
    this.m_Pmin = P_MIN;
    this.m_finalchunk = UINT_MAX;
    this.m_Samples = SAMPLES;

    this.m_firstDataName.clear();
    this.m_dataName.clear();

    this.m_winrecvd = 0;
    this.m_winpending = 0;
    this.m_curwindow = 1; //size_t m_pipeSize;
    this.m_firstInFlight = 0;
    this.m_pending_interest_count = 0;
    this.m_next_pending_interest = 0; //size_t m_nextSegment;
    this.m_finalSlot = UINT_MAX;

    this.m_interests_sent = 0;
    this.m_pkts_recvd = 0;
    this.m_pkts_bytes_recvd = 0;
    this.m_raw_data_bytes_recvd = 0;//size_t m_totalSize;
    this.m_pkts_delivered = 0;
    this.m_raw_data_delivered_bytes = 0;
    this.m_pkts_delivered_bytes = 0;
    this.m_isAbortModeEnabled = false;
    this.m_winretrans = 0;
    this.m_download_completed = false;
    this.m_reTxLimit = 0;
    this.m_nHoles = 0;
    this.m_old_npackets = 0;
    this.m_old_rate = 0.0;
    this.m_sent_interests = 0;
    this.m_old_interests = 0;
    this.m_old_data_inpackets = 0;
    this.m_interests_inpackets = 0;
    this.m_old_interests_inpackets = 0;
    this.m_isSending = false;
    this.m_isRetransmission = false;
    this.m_is_first = false;
    this.m_timeout = 0;        //system timeout, default to run forever
    this.m_nTimeouts = 0;
    this.m_cur_path = this.m_saved_path;
    this.m_PathTable.clear();
    this.insertToPathTable(DEFAULT_PATH_ID, this.m_saved_path);

    this.setStartTimetoNow();
    this.setLastTimeOutoNow();
};

NdnIcpDownload.prototype.onTimeout = function (interest) {
    console.log('timedout in expressing interest ' + interest.getName());
    if (!this.m_cur_path) {
        console.log('ERROR: when timed-out no current path found, exit');
        return;             //exit(EXIT_FAILURE);
    }

    //RAQM();

    var name = interest.getName();
    var timedOutSegment = name.components[name.size() - 1].toSegment();
    var slot = timedOutSegment % RECEPTION_BUFFER;

    // Check if it the data exists
    if (timedOutSegment >= this.m_finalchunk)
        return;

    //  check if data is received
    if (this.m_inFlightInterests[slot].checkIfDataIsReceived())
        return;

    this.m_inFlightInterests[slot].markAsReceived(false);
    var now = micro.now();          //time now
    this.m_inFlightInterests[slot].setSendTime(now);

    //  create proper interest
    var newInterest = interest;

    //  since we made a copy, we need to refresh interest nonce otherwise it could be considered as a looped interest by NFD.
    newInterest.refreshNonce();

    //re-express interest
    this.m_face.expressInterest(newInterest,
        this.onData.bind(this),
        this.onTimeout.bind(this));
    this.m_interests_sent++;
    this.m_last_timeout = micro.now();
    this.m_nTimeouts++;
};

NdnIcpDownload.prototype.onFirstData = function (interest, data) {
  console.log('ndn-icp-download.js: executing onFirstData ' + JSON.stringify(data));

    var name = data.getName();
    var first_copy = this.m_firstDataName;

    if (name.size() == this.m_firstDataName.size() + 1) {
        this.m_dataName = new Name(first_copy.append(name.components[name.size() - 1]));
    }
    else if (name.size() == this.m_firstDataName.size() + 2) {
        this.m_dataName = new Name(first_copy.append(name.components[name.size() - 2]));
    }
    else {
        console.log('ERROR: Wrong number of components.');
        return;
    }

    this.ask_more(this.m_next_pending_interest++);
};

NdnIcpDownload.prototype.ask_first = function () {
    console.log('ndn-icp-download.js: executing ask_first, m_firstDataName' + this.m_firstDataName);
    var interest = new Interest();

    interest = new Interest(new Name(this.m_firstDataName));
    interest.setInterestLifetimeMilliseconds(1000);


//    interest.setName(new Name(this.m_firstDataName));
//    interest.setInterestLifetimeMilliseconds(this.m_defaulInterestLifeTime);
//    interest.setMustBeFresh(true);
//    interest.setChildSelector(1);

    this.m_face.expressInterest(interest,
        this.onFirstData.bind(this),
        this.onTimeout.bind(this));
};

NdnIcpDownload.prototype.ask_more = function (seq) {
  console.log('ndn-icp-download.js: executing ask_more ' + seq);

    this.m_isSending = true;

    var slot = seq % RECEPTION_BUFFER;
    //assert(this.m_inFlightInterests[slot].getRawDataSize() == 0);
    var now = micro.now();
    this.m_inFlightInterests[slot].setSendTime(now).markAsReceived(false);

    //make a proper interest
    var interest = new Interest();
    interest.setName(new Name(this.m_dataName).appendSegment(seq));

    //TODO not sure if need to change to the lifetime given by option for timer or the timer for path at run time.
    //or simply the default lifetime given by option, here is confusing me
    //interest.setInterestLifetime(time::milliseconds(cur_path->getTimer()/1000));

    interest.setInterestLifetimeMilliseconds(this.m_defaulInterestLifeTime);
    // interest.setMustBeFresh(this.m_allow_stale);
    interest.setMustBeFresh(true);
    interest.setChildSelector(1);


    this.m_face.expressInterest(interest,
        this.onData.bind(this),
        this.onTimeout.bind(this));

    //TODO schedule a timeout after DEFAULT_INTEREST_TIMEOUT(or the one given by option for timer),bound to onTimeout
    this.m_interests_sent++;
    this.m_winpending++;
    //assert(this.m_pending_interest_count < RECEPTION_BUFFER);
};

NdnIcpDownload.prototype.download = function (name, initial_chunk, n_chunk) {    //rec_buffer removed
    console.log('executing ndn-icp-download.js: download');
    if (new Name(name) != new Name(this.m_firstDataName) && (this.m_firstDataName.size() !== 0)) {
      console.log('ndn-icp-download.js: download re_initializing..');

        this.re_initialize();
    }

    if (initial_chunk != -1) {
      console.log('ndn-icp-download.js: download initial_chunk !=-1');
        this.re_initialize();
        this.m_next_pending_interest = initial_chunk;
        this.m_firstInFlight = initial_chunk;
    }

    this.m_firstDataName = new Name(name);
    this.rec_buffer = null;

    if (n_chunk != -1 && this.m_finalchunk != UINT_MAX) {
      console.log('ndn-icp-download.js: download asking_more');

        this.m_finalchunk += n_chunk + this.m_next_pending_interest;
        this.ask_more(this.m_next_pending_interest++);
    }
    else if (n_chunk != -1 && this.m_finalchunk == UINT_MAX) {
      console.log('ndn-icp-download.js: download asking first ...');

        this.m_finalchunk = n_chunk + this.m_next_pending_interest;
        this.ask_first();
    }
    else {
      console.log('ndn-icp-download.js: download asking first');

        this.ask_first();
    }

    /*
    if (this.m_timeout < 4000 && this.m_timeout > 0)//this.m_timeout.count() < 4000 && ...??
        this.m_face.processEvents(this.m_timeout); // m_timeout is the system timeout??????????????????
    else {
        this.m_face.processEvents(4000);        ////////////////////////////////??????????????????

        if (this.m_pkts_delivered == 0) {
            console.log('ndn-icp-download: no data found:', this.m_dataName.toUri());
            return;             //throw std::runtime_error("No data found");
        }

        if (this.m_next_pending_interest < this.m_finalchunk) {
            if (this.m_timeout == 0) {              //this.m_timeout.count() == 0
                this.m_face.processEvents();            ///////////////////////processEvent
            }
            else {
                this.m_face.processEvents(this.m_timeout - 4000);       /m_next_pending_interest/milliseconds
            }

        }
    }

    if (this.m_download_completed) {
        this.re_initialize();
        return true;
    }
    else
        return false;
        */
};
