var micro = require('microseconds');

const TIMEOUT_SMOOTHER = 0.1;
const TIMEOUT_RATIO = 10;

var DataPath = function DataPath(dropFactor, Pmin, newTimer, Samples, NewRtt, NewRttMin, NewRttMax) {
    this.m_drop_factor = dropFactor;
    this.m_P_MIN = Pmin;
    this.timer = newTimer;
    this.m_Samples = Samples;
    this.m_rtt = NewRtt || 1000;
    this.m_rtt_min = NewRttMin || 1000;
    this.m_rtt_max = NewRttMax || 1000;
    this.m_drop_prob = 0;
    this.m_pkts_recvd = 0;
    this.m_last_pkts_recvd = 0;
    this.m_pkts_bytes_recvd = 0;           /** Total number of bytes received including ccnxheader*/
    this.m_last_pkts_bytes_recvd = 0;      /** pkts_bytes_recvd at the last path summary computation */
    this.m_raw_data_bytes_recvd = 0;       /** Total number of raw data bytes received (excluding ccnxheader)*/
    this.m_last_raw_data_bytes_recvd = 0;
    this.m_previous_call_of_path_reporter = micro.now();//this.m_previous_call_of_path_reporter = micro.now();

    this.m_RTTSamples = [];
};

exports.DataPath = DataPath;

DataPath.prototype.pathReporter = function () {
    var now = micro.now();
    var rate,
        delta_t;    //double

    delta_t = now - this.m_previous_call_of_path_reporter;
    rate = (this.m_pkts_bytes_recvd - this.m_last_pkts_bytes_recvd) * 8 / delta_t; // MB/s
    console.log('DataPath status report:', 'at time', now / 1000000, 'sec icp-download:', this, 'path;',
        'pkts_recvd ', (this.m_pkts_recvd - this.m_last_pkts_recvd), ';delta_t ', delta_t,
        ';rate ', rate, 'Mbps', ';RTT ', this.m_rtt, ';RTT_max ', this.m_rtt_max, ';RTT_min ', this.m_rtt_min);
    this.m_last_pkts_recvd = this.m_pkts_recvd;
    this.m_last_pkts_bytes_recvd = this.m_pkts_bytes_recvd;
    this.m_previous_call_of_path_reporter = micro.now();

    return this;
};

DataPath.prototype.insertNewRtt = function (newRTT) {
    this.m_rtt = newRTT;
    this.m_RTTSamples.push(newRTT);

    if (this.m_RTTSamples.length > this.m_Samples)
        this.m_RTTSamples.shift();
    this.m_rtt_max = Math.max.apply(null, this.m_RTTSamples);
    this.m_rtt_min = Math.min.apply(null, this.m_RTTSamples);
    return this;
};

DataPath.prototype.updateReceivedStats = function (packetSize, dataSize) {
    this.m_pkts_recvd++;
    this.m_pkts_bytes_recvd += packetSize;
    this.m_raw_data_bytes_recvd += dataSize;

    return this;
};

DataPath.prototype.getMyDropFactor = function () {
    return this.m_drop_factor;
};

DataPath.prototype.getDropProb = function () {
    return this.m_drop_prob;
};

DataPath.prototype.setDropProb = function (dropProb) {
    this.m_drop_prob = dropProb;
    return this;
};

DataPath.prototype.getMyPMin = function () {
    return this.m_P_MIN;
};

DataPath.prototype.getTimer = function () {
    return this.timer;
};

DataPath.prototype.smoothTimer = function () {
    this.timer = (1 - TIMEOUT_SMOOTHER) * this.timer + (TIMEOUT_SMOOTHER) * this.m_rtt * (TIMEOUT_RATIO);

    return this;
};

DataPath.prototype.getRtt = function () {
    return this.m_rtt;
};

DataPath.prototype.getRttMax = function () {
    return this.m_rtt_max;
};

DataPath.prototype.getRttMin = function () {
    return this.m_rtt_min;
};

DataPath.prototype.getMySampleValue = function () {
    return this.m_Samples;
};

DataPath.prototype.getRttQueueSize = function () {
    return this.m_RTTSamples.length;    //get<byArrival>().size();
};

DataPath.prototype.updateDropProb = function ()
{
    this.m_drop_prob = 0.0;

    if (this.getMySampleValue() == this.getRttQueueSize()) {
        if (this.m_rtt_max == this.m_rtt_min) {
            this.m_drop_prob = this.m_P_MIN;
        } else {
            this.m_drop_prob = this.m_P_MIN + this.m_drop_factor * (this.m_rtt - this.m_rtt_min) / (this.m_rtt_max - this.m_rtt_min);
        }
    }

    return this;
};
