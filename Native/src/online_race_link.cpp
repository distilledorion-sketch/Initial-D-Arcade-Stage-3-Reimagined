#include "online_race_link.h"
#include <stdexcept>

namespace idas3::original {
namespace {
constexpr std::uint32_t magic=0x31524c4fu;
void put(std::vector<std::uint8_t>& bytes,std::uint64_t value,unsigned width){for(unsigned i=0;i<width;++i)bytes.push_back(std::uint8_t(value>>(8*i)));}
struct Reader {
    std::span<const std::uint8_t> bytes;std::size_t offset=0;
    std::uint64_t get(unsigned width){if(offset+width>bytes.size())throw std::invalid_argument("Truncated authority packet");std::uint64_t value=0;for(unsigned i=0;i<width;++i)value|=std::uint64_t(bytes[offset++])<<(8*i);return value;}
};
}
OnlineRaceLink::OnlineRaceLink(OnlineRaceSimulation& simulation,std::uint64_t race,bool host,OnlineRaceTimeline::ConfirmedOutput output)
    :race_(race),timeline_(simulation,race,host,std::move(output)){}
std::vector<std::uint8_t> OnlineRaceLink::packet()const {
    const auto records=timeline_.outgoing(peerConfirmed_);
    std::vector<std::uint8_t> bytes;bytes.reserve(40+16*records.size());
    put(bytes,magic,4);put(bytes,1,4);put(bytes,race_,8);put(bytes,timeline_.confirmed(),8);
    put(bytes,timeline_.confirmedDigest(),8);put(bytes,records.size(),4);put(bytes,0,4);
    for(const auto& record:records){put(bytes,record.frame,8);put(bytes,record.slot,1);put(bytes,record.input.pressedByte,1);
        put(bytes,record.input.analog.steering,2);put(bytes,record.input.analog.throttle,2);put(bytes,record.input.analog.brake,2);}
    return bytes;
}
void OnlineRaceLink::receive(std::span<const std::uint8_t> bytes){
    if(bytes.size()<40||bytes.size()>1064)throw std::invalid_argument("Authority packet size outside bounds");
    Reader reader{bytes};if(reader.get(4)!=magic||reader.get(4)!=1||reader.get(8)!=race_)throw std::invalid_argument("Foreign authority packet");
    const auto ack=reader.get(8),digest=reader.get(8),count=reader.get(4);
    if(reader.get(4)!=0||count>64||bytes.size()!=40+16*count||ack>timeline_.frame()||(!ack&&digest))throw std::invalid_argument("Malformed authority acknowledgement");
    std::vector<OnlineInputRecord> records;records.reserve(std::size_t(count));
    for(unsigned i=0;i<count;++i){OnlineInputRecord r;r.race=race_;r.frame=reader.get(8);r.slot=unsigned(reader.get(1));r.input.pressedByte=std::uint8_t(reader.get(1));
        r.input.analog={std::uint16_t(reader.get(2)),std::uint16_t(reader.get(2)),std::uint16_t(reader.get(2))};r.input.calibration={128,32,32};records.push_back(r);}
    if(!timeline_.receive(records))throw std::invalid_argument("Authority input history rejected");
    if(ack>peerConfirmed_){peerConfirmed_=ack;peerDigest_=digest;}
}
void OnlineRaceLink::verify(){
    if(peerConfirmed_>verified_&&timeline_.confirmed()>=peerConfirmed_){
        if(timeline_.digestAfter(peerConfirmed_-1)!=peerDigest_)throw std::runtime_error("Authoritative race desynchronized; finish without awards");
        verified_=peerConfirmed_;
    }
}
void OnlineRaceLink::reconcile(){timeline_.reconcile();verify();}
bool OnlineRaceLink::step(const OriginalVehicleInputs& local){
    reconcile();if(!timeline_.localInput(local))return false;const bool advanced=timeline_.advance();verify();return advanced;
}
}
