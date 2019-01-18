var PendingInterest = function PendingInterest() {
    this.m_raw_data = null;
    this.m_raw_data_size = 0;
    this.m_packet_size = 0;
    this.m_is_received = false;
    this.m_send_time = null;
};

exports.PendingInterest = PendingInterest;

PendingInterest.prototype.getSendTime = function ()
{
    return this.m_send_time;
};

PendingInterest.prototype.setSendTime = function (now) {
    this.m_send_time = now;
    return this;
};

PendingInterest.prototype.getRawDataSize = function () {
    return this.m_raw_data_size;
};

PendingInterest.prototype.getPacketSize = function () {
    return this.m_packet_size;
};

PendingInterest.prototype.markAsReceived = function (val) {
    this.m_is_received = val;

    return this;
};

PendingInterest.prototype.checkIfDataIsReceived = function () {
    return this.m_is_received;
};

PendingInterest.prototype.addRawData = function (data) {
    if (this.m_raw_data !== null && this.m_raw_data !== undefined)
        this.removeRawData();

    var content = data.getContent();
    this.m_raw_data_size = content.size();
    this.m_packet_size = data.wireEncode().size();
    this.m_raw_data = new Uint8Array(this.m_raw_data_size);

    this.m_raw_data.set(content.buf());

    return this;
};

PendingInterest.prototype.getRawData = function ()
{
    if (this.m_raw_data !== null)
        return this.m_raw_data;
    else return null;
};

PendingInterest.prototype.removeRawData = function ()
{

    if (this.m_raw_data !== null) {
        this.m_raw_data = null;
        this.m_raw_data_size = 0;
        this.m_packet_size = 0;
    }

    return this;
};

